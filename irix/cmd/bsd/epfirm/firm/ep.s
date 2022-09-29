;E-Plex 8-port Ethernet firmware

; Copyright 1994 Silicon Graphics, Inc.  All rights reserved.
; "$Revision: 1.17 $"

    ;	.equ	CACHEOFF,1
    ;	.equ	DEBUGDMA,1
    ;	.equ	DEBUGLINK,1

 .ifdef ep_c.o
    ;rev C DANG chip
	.file	"ep_c.s"
 .else
   .ifdef ep_b.o
    ;rev B DANG chip
	.equ	DANG_BUG,1	;must read between all writes with bad DANG
   .else
	.err
   .endif
 .endif

	.equ	AM29030,1
	.include "std.h"
	.include "ephwd.h"
	.include "if_ep_s.h"
	.include "ep.h"

	.sect	code,text,absolute GIORAM
	.use	code

	.equ	VERS_FAM, 1 * 0x10000000
	.equ	VERS,VERS_FAM + (((VERS_Y-92)*13+VERS_M)*32+VERS_D)*24+VERS_H

	.sect	bss, bss, absolute GIORAM+EP_FIRM_SIZE
	.use	bss


;host buffer pools
;   Each pool of host buffers is a power of 2 entries long to make it
;   easy to wrap from the end back to the start.
;
;   Each pool consists of two blocks of the sames size.  Each entry
;   consists of two words, one in each block.  Both words have the same
;   index from the start of their blocks.  The word in the first block
;   contains the least significant 32 bits of the E-bus address.  The word
;   in the second block consists of most significant E-bus address bits
;   in bits 16-23 (see H2B_ADDR_HI_LSB) and junk elsewhere.

	DEFBT	MAX_POOL,   8
	.equ	POOL_LEN, MAX_POOL*4
	.equ	POOL_ADDR_HI_OFF, POOL_LEN
	.equ	BUF_BIT_MASK, POOL_LEN
	CKALIGN	BUF_BIT_MASK
	CK	MAX_POOL>=MAX_BIG && MAX_POOL>=MAX_SML
smls:	.block	POOL_LEN*2
bigs:	.block	POOL_LEN*2

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
;   t1=OP:ADDR_HI:S1, t2=ADDR
;   t1,t3 changed
 .macro SAVBUF,typ,branch
	  store	  0,WORD,t2,lst_@typ@
	  CK	  POOL_ADDR_HI_OFF == BUF_BIT_MASK
	  add	  t3,lst_@typ@,buf_bit_mask
	  ADVPOOL lst_@typ@,typ
	  store	  0,WORD,t1,t3
	  CK	  H2B_ADDR_HI_MASK == 0xff
	  jmp	  branch
	   add	  @typ@_bufs,@typ@_bufs,1
 .endm

  ;allocate a buffer
;   of type typ, putting its address into reg2:reg1
 .macro USEBUF,typ,reg1,reg2,branch
	  load	  0,WORD,reg1,nxt_@typ@
	  add	  reg2,nxt_@typ@,buf_bit_mask
	  CK	  POOL_ADDR_HI_OFF == BUF_BIT_MASK
	  add	  reg2,reg2,(31-H2B_ADDR_HI_LSB_BT)/8
	  CK	  H2B_ADDR_HI_LSB_BT != 24 && (H2B_ADDR_HI_LSB_BT % 8) == 0
	  ADVPOOL nxt_@typ@,typ
	  load	  0,BYTE,reg2,reg2
	  CK	  H2B_ADDR_HI_MASK == 0xff
   .ifnes "@branch@",""
	  jmp	  branch
	   sub	  @typ@_bufs,@typ@_bufs,1
   .else
	  sub	  @typ@_bufs,@typ@_bufs,1
   .endif
 .endm

	.use	bss

;zeros for padding small packets and other useful purposes
zeros:	.block	    N_SUMREG*4
	CK	    N_SUMREG*4 > MIN_ETHER+TX_PAD+3

cmd_id:	.block	    4			;most recent host command ID

;address of host-to-board DMA control ring in host memory
h2b_base:.block  4

;address of board-to-host control queue on the host
b2h_h_base: .block  4

;total maximum host buffers
max_bufs:   .block  4


;command buffer or communications area
	DMABLK	host_cmd, HC_CMD_SIZE

;dummy DMA read
	DMABLK	dma_dummy, 4


;host-to-board commands
	.equ	H2B_BUF_NUM,  32
	.equ	H2B_BUF_SIZE, H2B_BUF_NUM * _SIZE_H2B_BLK
	.use	bss
	MALIGN	H2B_BUF_SIZE
	DMABLK	h2b_buf, H2B_BUF_SIZE
	.block	_SIZE_H2B_BLK


;board-to-host control queue
	.equ	BD_B2H_LEN, 60
	CK	BD_B2H_LEN < B2H_LEN
	.equ	BD_B2H_SIZE, BD_B2H_LEN*_SIZE_B2H_BLK
	MALIGN	BD_B2H_SIZE
	DMABLK	b2h_0, BD_B2H_SIZE
	DMABLK	b2h_1, BD_B2H_SIZE


;Counters sent to the host for ECMD_FET_CT
;   This is an array of blocks of counters, one block for each SONIC.
	.set	_SIZE_cnts0, 0
	.macro	CNT,nm
cnt_@nm:  .block  4
	  .set	_SIZE_cnts0, _SIZE_cnts0+4
	.endm
	.dsect
	CNTS
	.purgem	CNT
	.use	*

    ;The base address of each block of counters is computed by shifting
    ;the SONIC number.  So the overall size of the block must be a power
    ;of 2.
	DEFBT	SHIFT_CNTS0, 5+2
	CK	_SIZE_cnts0 == SHIFT_CNTS0
	.equ	_SIZE_cnts, _SIZE_cnts0*EP_PORTS

	DMABLK	cnts, _SIZE_cnts


;all host data DMA goes through these buffers
	.equ	_SIZE_tbounce, ETHER_SIZE+_SIZE_TXpkt+TX_PAD+DMA_ROUND
	MALIGN	_SIZE_tbounce,
	DMABLK	tbounce, _SIZE_tbounce

	.equ	_SIZE_rbounce,  RX_PAD+RPKT_SIZE+DMA_ROUND
	MALIGN	_SIZE_rbounce
	DMABLK	rbounce0, _SIZE_rbounce, 1024
	MALIGN	_SIZE_rbounce
	DMABLK	rbounce1, _SIZE_rbounce




;Each of the SONICs has an equal, 128KByte share of the 1MByte of static SONIC
;   RAM.  Each 128KByte portion is used identically.
;   use the SRAMADR macro to get the address of elements of this structure.
	.dsect
    ;CAM contents
_CAM:	.block	(2+3*2+2)*2

    ;Receive Descriptor Area (RDA)
_RXpkt:	.block	_SIZE_RXpkt * (NUM_RX_BUFS+1)

    ;Receive Resource Area (RRA)
	.equ	_SIZE_RXrsrcs, _SIZE_RXrsrc * NUM_RX_BUFS
_RXrsrc:.block	_SIZE_RXrsrcs
	.equ	_RXrsrc_last, _RXrsrc+_SIZE_RXrsrcs-_SIZE_RXrsrc

	CK	(. & 0xffff0000) == (_CAM & 0xffff0000)

    ;Receive Buffer Areas
    ; The buffer size should be a multiple of DANG_CACHE_SIZE, and must
    ; be known to the host.
	.equ	RBUF_SIZE, (RPKT_SIZE+RBUF_JUNK_SIZE+8)
_RXbufs:.block	RBUF_SIZE * NUM_RX_BUFS
	.block	4

    ;TX descriptors and buffers
	.equ	_SIZE_TXbufs, SONICRAM_SIZE - .
_TXbufs:.block	_SIZE_TXbufs
	CK	((.-1) & 0xffff0000 ) == (_TXbufs & 0xffff0000)

	.use	*


;Each SONIC has private data in the GIORAM.
;   Use the GRAMADR macro to get the address of elements of this structure.
;   Make its size a power of 2 ease indexing.
	.equ	_SIZE_sregs0, 4*N_SREG
	MALIGN	_SIZE_sregs0
spriv0:
sregs0:	.block	_SIZE_sregs0		;registers for each SONIC

    ;multicast and promiscuous DA hash table for a SONIC
	.equ	DAHASH_SIZE, 256/8	;256 bits in the table
	DMABLK	dahash0, DAHASH_SIZE

old_mpt0:.block	4

    ;Each host-to-board transmit data DMA request is DMA'ed from the host to
    ;   this buffer of blocks.  There must be at least one free entry for
    ;	the end-of-queue flag.
	.set	D2B_SIZE, (MAX_OUTQ+1) * (MAX_DMA_ELEM+1) * _SIZE_H2B_BLK
d2b0:	.block	D2B_SIZE
d2b_end0:.block	(MAX_DMA_ELEM+1) * _SIZE_H2B_BLK
	.block	4			;end flag

    ;Allocate a power-of-2 sized block to ease finding the right block
    ;of per-SONIC data.
	.equ	_SIZE_SGRAM0, .-sregs0	;bytes actually needed
	DEFBT	_SIZE_SGRAM, 12		;bytes allocated
	CK	_SIZE_SGRAM0 < _SIZE_SGRAM
	CK	_SIZE_SGRAM0 >= _SIZE_SGRAM/2

	.block	spriv0+_SIZE_SGRAM*EP_PORTS-.
    .irep	n, 1,2,3,4,5,6,7
	.equ	spriv@n, spriv0+n*_SIZE_SGRAM
	.equ	dahash@n, dahash0+n*_SIZE_SGRAM
	.equ	old_mpt@n, old_mpt0+n*_SIZE_SGRAM
	.equ	d2b@n, d2b0+n*_SIZE_SGRAM
	.equ	d2b_end@n, d2b_end0+n*_SIZE_SGRAM
    .endr


end_of_giomem:



;Execution starts here.
;   This is assembled as if at GIORAM=0x080000, but executed at EPROM=0.
;   This works because some 29k jumps are PC-relative.  As soon as the
;   code has been copied to GIORAM, we jump to it and run from there.

;To save the cost of aligning the 29K interrupt vector table, it is
;   located at the start of GIORAM.  To simplify that, it is located
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

    ;set a few global registers
	constx	buf_bit_mask,	BUF_BIT_MASK
	constx	p_gsor,		GSOR
	constx	p_intc,		INTC
	constx	p_host_cmd,	host_cmd

    ;reset the SONICs
	constn	v0,-1
	sto	v0,SONIC_RESET,v1


;Get the address of the start of host communication area
;   Treat v0:host_cmd_buf:v2:v3 as one big register.  Bits are shifted into
;   the most significant bit of v0 and out of the least significant of v3.
	constx	t0,LEDS
	constx	t1,LED_YELLOW		;say we are doing nibble mode
	store	0,WORD,t1,t0
gethc1:	constx	v0,0x11111111
	add	host_cmd_buf,v0,v0	;start with something different from
	add	v2,host_cmd_buf,v0	;   the preamble
	add	v3,v2,v0

gethc2:	load	0,WORD,t0,p_gsor
	tbit	t1,t0,INTC_H2BINT
	jmpf	t1,gethc2		;wait for an interrupt request

	 srl	t0,t0,GSOR_FLAG0_BT
	mtsrim	FC,32-4
	extract	v3,v2,v3		;move 4 bits from word to word
	extract	v2,host_cmd_buf,v2
	extract	host_cmd_buf,v0,host_cmd_buf
	and	t0,t0,0xf
	extract	v0,t0,v0		;put 4 bits from host into 1st word

	constx	t0,INTC_H2BINT		;reset the host-to-board interrupt
	store	0,WORD,t0,p_intc

	constx	t0,EPB_GIO_DL_PREAMBLE	;wait for the magic word
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

	constx	t0,LEDS
	const	t1,0			;say nibble mode is over
	store	0,WORD,t1,t0

    ;XXX do something with the opcode in v2


;   clear RAM
    .if	N_SUMREG > 64
	.equ	CLR_LEN, 64
    .else
	.equ	CLR_LEN, N_SUMREG
	CK	CLR_LEN > 2
    .endif
	constx	t0,GIORAM_SIZE/CLR_LEN/4-2
	constx	t1,GIORAM-CLR_LEN*4
	const	t2,CLR_LEN*4
ramclr1:mtsrim	CR,CLR_LEN-1
	add	t1,t1,t2
	jmpfdec	t0,ramclr1
	 storem	0,WORD,sumreg,t1

	constx	t0,SONICRAM_SIZE*EP_PORTS/CLR_LEN/4-2
	constx	t1,SONICRAM03-CLR_LEN*4
	const	t2,CLR_LEN*4
ramclr2:mtsrim	CR,CLR_LEN-1
	add	t1,t1,t2
	jmpfdec	t0,ramclr2
	 storem	0,WORD,sumreg,t1

;   Checksum part of the EPROM as we copy it to GIORAM
	const	v0,0			;v0=accumulating checksum
	constx	t0,EPROM
	constx	t1,GIORAM-4
	constx	t2,EP_FIRM_SIZE/4-2
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
	constx	t2,(EPROM_SIZE-EP_FIRM_SIZE)/4/2-2
	const	t3,0
ckprom4:load	0,WORD,t4,t0
	add	t0,t0,4
	add	v0,v0,t3
	load	0,WORD,t3,t0
	add	t0,t0,4
	jmpfdec	t2,ckprom4
	 add	v0,v0,t4
	add	v0,v0,t3


;   If the checksum is bad, continue in the hope that enough is ok to do
;   a new download.
	cpneq	t0,v0,0
	c2set_st t0,BSUM

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
	mtsrim	CR,LEN_VEC-1		;install the interesting vectors
	storem	0,WORD,sumreg,t3

	mtsr	VAB, v0			;start using the interrupt table


    ;Tell the host we are alive by writing our signature in the host
    ;communication area.  If the checksum was bad, say so in the version.
	add	v4,p_host_cmd,hc_sign
	constx	t1,EPB_SIGN
	store	0,WORD,t1,v4

	constx	t1,VERS
	t_st	t3,BSUM
	cplt	t3,t3,0
	CK	CKSUM_VERS == TRUE
	or	t1,t1,t3		;put bad checksum bit in sign bit

	add	t0,p_host_cmd,hc_vers
	store	0,WORD,t1,t0

	DODMA	8,B2H, v4,0, 0,host_cmd_buf,hc_sign, t0,t1



    ;clean the host buffer pools
	constx	nxt_big,bigs
	mov	lst_big,nxt_big
	constx	big_bufs,-MIN_BIG

	constx	nxt_sml,smls
	mov	lst_sml,nxt_sml
	constx	sml_bufs,-MIN_SML

	constx	b2h_b_base,b2h_0
	add	b2h_b_lim,b2h_b_base,BD_B2H_SIZE
	add	b2h_b_ptr,b2h_b_base,0

    ;start the 29K timer
	constx	v0,TIME_INTVL
	mtsr	TMC,v0
	CK	TMR_IE > 0xffff
	consth	v0,TIME_INTVL | TMR_IE
	mtsr	TMR,v0

    ;turn off unneeded interrupts
	constx	gsor,GSOR_HIE
	store	0,WORD,gsor,p_gsor

    ;set the rest of the global registers
	constx	p_rbounce,rbounce0
	constx	p_cnts,cnts
	constx	p_spriv0,spriv0
	constx	p_sram,SONICRAM03

	set_st	H2B_SLEEP
	set_st	HINT_DONE
	consth	h2b_atime,TIME_NEVER
	consth	h2b_delay,TIME_NEVER

	constx	max_obf_avail, _SIZE_TXbufs - (_SIZE_TXpkt+TX_PAD+MIN_ETHER)

    ;clear the per-SONIC 29K registers
	constx	v0,zeros
	mtsrim	CR,N_SREG-1
	loadm	0,WORD,%%SREG0,v0
	const	v0,0
	constx	v1,SONIC0
	constx	d2b_len,D2B_END
clr_sregs:
	add	sn,v0,0
	constx	p_isr,SONIC0+SN_ISR
	sll	t1,sn,SONIC_INC_BT
	add	p_isr,p_isr,t1
	STOREGS	t1,all
	cpeq	t0,sn,EP_PORTS-1
	jmpf	t0,clr_sregs
	add	v0,sn,1

    ;reset the SONICs
	 constn	snreset,-1
	sto	snreset,SONIC_RESET,v1

;for debugging, turn off the caches
 .ifdef CACHEOFF
	constx	v0, CFG_PMB_16K | CFG_ILU | CFG_ID
	mtsr	CFG, v0
 .endif

;Go run the code in GIORAM after we finally turn on interrupts
;   Interrupts must not be turned on before this point.
	const	v0,mlop_cmd
	consth	v0,mlop_cmd
	jmpi	v0
	 mtsrim	CPS,PS_INTSON


;The preceding code is overwritten with the interrupt vectors after it
;has been copied from EEPROM to GIORAM.
 .if . < NUM_V*4+GIORAM
	.block	NUM_V*4+GIORAM - .
 .endif





;Trap and interrupt vectors
intvecs:
	.word	timerint		;V_TIMER
	.word	badint
	.word	badint			;V_INTR0    SONICs
	.word	badint			;V_INTR1    DMA complete
	.word	hostint			;V_INTR2    host to board
	.word	badint			;V_INTR3    unused
	.word	badint			;V_TRAP0    unused
	.word	badint			;V_TRAP1    unused

	.equ	LEN_VEC, (. - intvecs)/4


	.use	code

h2b_tbl:
	.word	h2b_emp			;0
	.word	h2b_wrap		;1 back to start of buffer
	.word	h2b_sml			;2 add little mbuf to pool
	.word	h2b_big			;3 add cluster to pool
	.rep	EP_PORTS
	.word	h2b_o			;4-11 start output
	.endr
	.word	h2b_o_cont		;12 continue output
	.word	h2b_o_end		;13 end output
	.word	h2b_full


;host command table
	.align
cmd_tbl:.word	cmd_done		;00	NOP
	.word	cmd_init		;01	set board parameters

	.word	cmd_sto			;02	download to the board
	.word	cmd_exec		;03	execute downloaded code
	.word	cmd_fet			;04	upload from the board

	.word	cmd_mac_parms		;05	set MAC
	.word	cmd_mac_off		;06	turn off a SONIC

	.word	cmd_wakeup		;07	wake up H2B DMA

	.word	cmd_fet_cnt		;08	fetch counters

	.word	cmd_poke		;09
cmd_tbl_end:





;start running in GIORAM here
	.use	code

;main loop
;   stay in this loop forever
	.use	code
	.align	16
;resume work on SONIC v0
mlop_sonic0:
	SELREGS	v2,t1,

;come back here to continue working on the same SONIC
mlop_sonic:
	jmpt	g_st,cmd		;handle an host command
	 CK	GST_MB == TRUE

    ;DMA an input packet to the host if we have enough host buffers and
    ;there is some pending input from this SONIC.
	 tbit	t0,isr,ISR_PKTRX
	or	t2,sml_bufs,big_bufs
	andn	t0,t0,t2
	jmpt	t0,rx

    ;If we have output space and pending output, DMA from the host
	 cpge	v3,obf_avail,d2b_len	;v3=output ready & have enough space
	andn	v3,v3,tx_lim		;v3=and not too much output
	jmpt	v3,tx

    ;handle less common SONIC interrupts
	 const	v1,ISR_TXDN|ISR_PINT | ISR_MP|ISR_RFO|ISR_RBAE|ISR_BR|ISR_LCD
	and	v1,v1,isr
	cpeq	t0,v1,0
	jmpt	t0,mlop_get_isr9

    ;SONIC has finished all output
	 tbit	t0,isr,ISR_TXDN
	jmpt	t0,tx_stop
    ;SONIC has finished some output
	 tbit	t0,isr,ISR_PINT
	jmpt	t0,tx_fin

    ;count FIFO overruns
	 BITCNT	rx_rfo,ISR_RFO,isr,,t1,t2
    ;count packets too large
	BITCNT	rx_rbae,ISR_RBAE,isr,,t1,t2
    ;count bus retries
	BITCNT	br,ISR_BR,isr,,t1,t2

    ;notice if LCD is finished so that we can start output
	mv_bt	t0,ISR_PINT, isr,ISR_LCD
	const	t1,ISR_PINT
	and	t0,t0,t1
	or	isr,isr,t0
	mv_bt	t2,SNST_LCD_WAIT, t0,ISR_PINT
	andn	sn_st,sn_st,t2

    ;count missing heartbeat
;XXX	 tbit	t0,isr,ISR_HBL
;XXX	jmpt	t0,XXX
;asdf count it or something, and then turn off
;the interrupt when it seems the transceiver does not
;do heartbeat.  But does this make sense for 10BASE-T?
;XXX XXX:

    ;count missed packets
	 tbit	t0,isr,ISR_MP
	jmpf	t0,mlop_get_isr5
	 const	t0,ISR_MP
	andn	isr,isr,t0
	SNADR	v4,MPT			;v4=address of SONIC counter
	load	0,HWORD,v5,v4		;v5=current tally
	GRAMADR	v6,old_mpt,t0		;v6=address old value
	consth	v5,0x10000		;ISR said the counter rolled
	load	0,WORD,v8,v6		;v8=old value
	sub	v8,v5,v8		;v8= new value - old value
	UPCNT	rx_mp,v8,t0,t1		;update the counter

	consth	v5,0
	store	0,WORD,v5,v6		;save the new old value of counter
mlop_get_isr5:
	andn	isr,isr,v1
mlop_get_isr9:

    ;If recent GSOR interrupt request bits say the SONIC wants
    ;attention, then fetch the SONIC interrupt status register.
	 subr	t1,sn,31-GSOR_SIR0_BT
	sll	t0,gsor,t1
	jmpf	t0,mlop_done		;next SONIC if no pending interrupts

	 constx	t3,GSOR_SIR0
	load	0,HWORD,t1,p_isr	;t1=raw interrupt bits
	sll	t3,t3,sn
	andn	gsor,gsor,t3

	const	t0,IMR_BITS		;t0=mask for interesting bits
	or	isr,isr,t1
	store	0,HWORD,isr,p_isr	;reset the bits in the SONIC

	and	isr,isr,t0		;discard uninteresting bits

	and	t1,isr,ISR_RDE|ISR_RBE
	cpneq	t2,t1,0
	srl	t2,t2,31-ISR_PKTRX_BT	;check for input after some errors
	jmp	mlop_sonic
	 or	isr,isr,t2



    ;Finished with this SONIC, at least for now.
mlop_done:

    ;forget the SONIC if it is happy
    ;Do not mark it inactive it it is  output-throttled, since we cannot
    ;be sure it is happy.
	jmpt	tx_lim,mlop_next
	 cpeq	v1,isr,0
	srl	v1,v1,31-SNACT0_BT
	sll	v1,v1,sn
	andn	snact,snact,v1

    ;pick the next SONIC
mlop_next:
	constn	t0,-1
	sll	t0,t0,sn
	andn	t1,snact,t0
	clz	v2,t1
	subr	v2,v2,31-SNACT0_BT	;v2=next SONIC or -1
	jmpf	v2,mlop_sonic0
	 nop


    ;After a pass through SONICs, do some housekeeping
    ;Also come here after doing a host command.  Come here instead of
    ;mlop_sonic in case the current SONIC was turned off.
mlop_cmd:
	jmpt	g_st,cmd		;do an host command
	 CK	GST_MB == TRUE

	 mfsr	v1,TMC
	cple	t1,v1,h2b_delay
	jmpt	t1,h2b			;Do H2B control DMA if it is time

	 t_st	t0,NEEDI,
	jmpt	t0,inth			;cause host interrupt

    ;If all recently SONIC interrupts have been serviced, look for new ones.
	cpeq	t0,snact,0
	jmpf	t0,mlop_cmd3
	 nop

	load	0,WORD,t0,p_gsor
	andn	t0,t0,snreset		;get new SONIC interrupt requests
	or	gsor,gsor,t0
	or	snact,snact,t0		;mark interrupting SONICs active

mlop_cmd3:
	clz	v2,snact
	subr	v2,v2,31-SNACT0_BT	;v2=next SONIC or -1
	jmpf	v2,mlop_sonic0
	 const	tx_lim,TX_LIM_RESET

	jmp	mlop_cmd		;spin if no SONIC is active
	 nop




;Cause a host interrupt
inth:
    ;first force pending B2H DMA to ensure host has heard all we have to say
	t_st	t0,OUT_DONE
	callt	t0,v0,host_fin_out

	cpneq	t0,b2h_b_ptr,b2h_b_base
	callt	t0,t4,dob2h

    ;Do an extra DMA to flush the DANG cache or write buffer
    ;You must either do two extra writes or one extra read.
	t_st	t0,DMA_CACHE
	jmpf	t0,inth6
	 rs_st	DMA_CACHE
	DUMDMA	t0,t1

	WAITDMA	t0,fast

    ;zap the host, after the DMA is finished
inth6:
	andn	g_st,g_st, GST_NEEDI | GST_OUTINT_REQ
	constx	t0,INTC_B2HINT
	jmp	mlop_cmd
	 store	0,WORD,t0,p_intc	;poke the host



;DMA control info from the host to the board
;   here v1=current time
;
;   Always copy a fixed number of words, relying on an empty sentinel
;   after the end of the real slots.
h2b:
	cpeq	h2b_delay,h2b_pos,0	;have things been initialized?
	jmpt	h2b_delay,mlop_cmd	;no, the host is not ready for us yet
	 CK	TIME_NEVER == TRUE

	 constx	v5,h2b_buf		;v5=start of buffer

	constx	v1,H2B_BUF_SIZE		;compute size to fetch, ending at
	CK	DANG_CACHE_SIZE<=H2B_BUF_SIZE    ;end of cache line
	and	t0,h2b_pos,DANG_CACHE_SIZE-1
	sub	v1,v1,t0		;v1=bytes to read

	DODMA	v1,H2B, v5,0, 0,h2b_pos,0, t0,t1

	rs_st	DMA_CACHE		;reads force the DANG cache

	add	v6,v5,0			;v6=pointer into H2B board buffer
	add	v7,v5,v1		;v7=end of buffer
	consth	t0,EPB_H2B_LAST<<H2B_OP_BT
	store	0,WORD,t0,v7

	mfsr	h2b_delay,TMC
	sub	h2b_delay,h2b_delay,h2b_dma_delay  ;schedule next H2B DMA

    ;acknowledge output or announce input instead of waiting for DMA to end
	t_st	t0,OUT_DONE
	const	v0,h2b_lop
	jmpt	t0,host_fin_out
	 consth	v0,h2b_lop

	cpneq	t0,b2h_b_ptr,b2h_b_base
	jmpt	t0,dob2h
	 add	t4,v0,0

	WAITDMA	t0,fast			;just wait if nothing else to do

    ;process requests from the host
    ;v5=start of board buffer, v6=current entry
h2b_lop:
	mtsrim	CR,_SIZE_H2B_BLK/4-1
	CK	_H2B_OP == 0
	CK	&t1+1 == &t2
	CK	_SIZE_H2B_BLK == 2*4
	loadm	0,WORD,t1,v6		;t1=OP:ADDR_HI:S1, t2=ADDR or S2:S3

	constx	t3,h2b_tbl
	srl	t4,t1,H2B_OP_BT-2
	and	t4,t4,H2B_OP_MASK*4
	add	t3,t3,t4
	load	0,WORD,t3,t3
    ;go to the function with t1=OP:ADDR_HI:S1, t2=ADDR or S2:S3,
    ;	t4=shifted opcode
	add	h2b_pos,h2b_pos,_SIZE_H2B_BLK
	jmpi	t3
	 add	v6,v6,_SIZE_H2B_BLK


;save new small buffer
h2b_sml:
	SAVBUF	sml,h2b_lop

;save new big buffer
h2b_big:
	SAVBUF	big,h2b_lop

;back to the start of the buffer
h2b_wrap:
	constx	t1,h2b_base
	jmp	h2b_full1
	 load	0,WORD,h2b_pos,t1	;wrap to start of host buffer

;The host more than filled our buffer
h2b_full:
	sub	h2b_pos,h2b_pos,_SIZE_H2B_BLK	;this opcode was not from host
h2b_full1:
	rs_st	HINT_DONE,		;re-enable board-to-host interrupt
	mfsr	h2b_atime,TMC
	sub	inth_time,h2b_atime,h2b_dma_delay
	jmp	h2b			;read again immediately
	 sub	h2b_atime,h2b_atime,h2b_sleep


;start of output request
;   t1=OP:sn:totlen, t2=offsum:cksum, t4=shifted opcode
;   The least significant 3 bits of the operation encode the SONIC #.
h2b_o:
	srl	t3,t4,2			;remove shift for jump table
	sub	t3,t3,EPB_H2B_O		;t3=SONIC #
	SELREGS t3,t0			;switch to the correct SONIC

	subr	t0,sn,31-SONIC_RESET0_BT
	sll	t0,snreset,t0		;been reset since host queued request?
	jmpt	t0,h2b_lop		;yes, forget it

	 constx	t0,SNACT0
	sll	t0,t0,sn
	or	snact,snact,t0		;mark the SONIC active

    ;Put the start of a new output job into the queue.  This includes
    ;over-writing the end-of-queue flag written after the previous job
    ;with our start-of-job flag.
	cpeq	t0,t0,t0		;t0=D2B_START
	CK	D2B_START == TRUE
	or	t1,t1,t0
	CK	H2B_OP_MASK <= 0x3f
	CK	EPB_H2B_LAST <= H2B_OP_MASK
	mtsrim	CR,_SIZE_H2B_BLK/4-1
	CK	_SIZE_H2B_BLK/4 == 2
	storem	0,WORD,t1,d2b_tail	;save info for starting packet
	add	d2b_tail,d2b_tail,_SIZE_H2B_BLK

    ;If SONIC TX is not running, then set the length of the next output
    ;packet so that it will start running when we have finished dealing
    ;with all of the current series of host-to-board requests, including
    ;the continuations of this output request.
	tbit	t0,d2b_len,D2B_END	;is SONIC TX running?
	jmpf	t0,h2b_lop

	 consth	t1,0			;no, start SONIC on 1st job
	CK	H2B_LEN_MSB_BT <= 15
	jmp	h2b_lop
	 add	d2b_len,t1,0		;d2b_len=total packet length


;continue to note output request
;   This assumes the entire request is complete in the host-to-board
;	area on the host and that the firmware will pick up all of the
;	pieces before starting to work on the output.
;   t1=OP:sn:high E-bus address bits:length, t2=32-bits of E-bus address,
h2b_o_cont:
	subr	t0,sn,31-SONIC_RESET0_BT
	sll	t0,snreset,t0		;been reset since host queued request?
	jmpt	t0,h2b_lop		;yes, forget it

	sll	t1,t1,31-H2B_ADDR_HI_MSB_BT
	srl	t1,t1,31-H2B_ADDR_HI_MSB_BT ;t1=E-bus addr bits:fragment len
	CK	H2B_LEN_MSB_BT <= 15
 .ifdef DEBUGDMA
	srl	t0,t1,H2B_ADDR_HI_LSB_BT
	cpneq	t0,t0,0			;for now, no high E bits should be on
	jmpt	t0,h2b_o_bad
	 nop
 .endif
	mtsrim	CR,_SIZE_H2B_BLK/4-1	;store fragment info
	storem	0,WORD,t1,d2b_tail
	jmp	h2b_lop
	 add	d2b_tail,d2b_tail,_SIZE_H2B_BLK

 .ifdef DEBUGDMA
;look for DMA corruption in bad E-bus address.
h2b_o_bad:
	constx	t0,LEDS
	constx	t1,LED_YELLOW | LED_GREEN1 | LED_GREEN2
	store	0,WORD,t1,t0
	mtsrim	CPS,PS_INTSOFF
	jmp	.
	 halt
	nop
 .endif

;note end of output request
;   This assumes the entire request is complete in the host-to-board
;	area on the host and that the firmware will pick up all of the
;	pieces before starting to work on the output.
;   t1=OP:sn:high E-bus address bits:length, t2=32-bits of E-bus address,
h2b_o_end:
	subr	t0,sn,31-SONIC_RESET0_BT
	sll	t0,snreset,t0		;been reset since host queued request?
	jmpt	t0,h2b_lop		;yes, forget it

	sll	t1,t1,31-H2B_ADDR_HI_MSB_BT
	srl	t1,t1,31-H2B_ADDR_HI_MSB_BT ;t1=E-bus addr bits:fragment len
	CK	H2B_LEN_MSB_BT <= 15
 .ifdef DEBUGDMA
	srl	t0,t1,H2B_ADDR_HI_LSB_BT
	cpneq	t0,t0,0			;for now, no high E bits should be on
	jmpt	t0,h2b_o_bad
	 nop
 .endif
	consth	t3,D2B_START+D2B_END	;t3=end flag
	mtsrim	CR,_SIZE_H2B_BLK/4+1-1	;store fragment info plus end flag
	storem	0,WORD,t1,d2b_tail
	add	d2b_tail,d2b_tail,_SIZE_H2B_BLK

	cpge	t0,d2b_tail,d2b_lim	;wrap to start of queue at its end
	jmpf	t0,h2b_lop
	 nop
	GRAMADR	d2b_tail,d2b,t0
	jmp	h2b_lop
	 store	0,WORD,t3,d2b_tail


;We have finished processing the requests read from the host.
;   If the host had nothing to say for a long time, stop doing host-to-board
;	DMA and tell the host.
;   If the host said more than fit in our buffer, then arrange to immediately
;	do another DMA.
;   If the host should have said something but did not, then interrupt it
;	and set the DMA delay to about the interrupt latency of the host.


;The host said something
h2b_act:
	rs_st	HINT_DONE,		;re-enable board-to-host interrupt
	mfsr	h2b_atime,TMC

	sub	inth_time,h2b_atime,h2b_dma_delay
	jmp	mlop_cmd
	 sub	h2b_atime,h2b_atime,h2b_sleep


;The host did not fill the buffer
;   v5=start of buffer, v6=last read
h2b_emp:
	sub	v6,v6,v5
	cpgt	t3,v6,_SIZE_H2B_BLK	;did the host say anything at all?
	jmpt	t3,h2b_act		;yes
	 sub	h2b_pos,h2b_pos,_SIZE_H2B_BLK	;will re-read empty slot

;host was mute
;   If we do not have the maximum number of buffers, then the host may
;   not have noticed the most recent input frames.  If it seems the
;   host is inactive and has not noticed the recent input, interrupt it.

	lod	t0,max_bufs
	add	t1,sml_bufs,big_bufs	;has host noticed all input?
	cpge	t2,t1,t0
	jmpt	t2,h2b_emp4		;yes, forget the interrupt
	 mfsr	v2,TMC			;v2=current time

	cple	t0,v2,inth_time
	jmpf	t0,h2b_emp5		;are we ready to interrupt the host?

	 t_st	t1,HINT_DONE		;yes, has the host heard the input?
	jmpt	t1,h2b_emp5
	 nop

	sub	h2b_atime,v2,h2b_sleep	;no, keep H2B DMA active
	or	g_st,g_st,GST_NEEDI|GST_HINT_DONE   ;request interrupt
	jmp	mlop_cmd
	 sub	inth_time,v2,int_delay	;no more interupts for a while


;The host has acknowledged all input.
;   v2=currrent time
h2b_emp4:
	rs_st	HINT_DONE,

;time to just wait
;   v2=currrent time
h2b_emp5:
	cplt	t0,h2b_atime,v2
	jmpt	t0,mlop_cmd		;wait for next H2B DMA if awake

;   If we are about to go to sleep, tell the host.  There must be another
;   H2B DMA, which will cause this code to check a final time to see if
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

	t_st	t0,H2B_SLEEP
	jmpt	t0,h2b_emp9
	 set_st	H2B_SLEEP

	call	t4,addb2h
	 consth	t0,B2H_SLEEP
	 CK	(B2H_SLEEP & 0xffff) == 0
	jmp	mlop_cmd
	 rs_st	HINT_DONE,		;re-enable host interrupt


;The host has been told of all input and that we are going to sleep.
;We have done a H2B DMA after announcing our sleepiness.
;The host appears to be asleep.
;Interrupt the host if it has not heard of its finished output.
h2b_emp9:
	mv_bt	t0,GST_NEEDI, g_st,GST_OUTINT_REQ
	and	t0,t0,GST_NEEDI
	or	g_st,g_st,t0
	consth	h2b_atime,TIME_NEVER
	jmp	mlop_cmd
	 consth	h2b_delay,TIME_NEVER



;put an entry into the B2H ring
;   entry: t0=word for the host,  t4=return
;   exit: t0-t3 changed
addb2h:
	add	b2h_sernum,b2h_sernum,1
	sll	t1,b2h_sernum,B2H_SERNUM0_BT
	or	t0,t0,t1
	store	0,WORD,t0,b2h_b_ptr
	add	b2h_b_ptr,b2h_b_ptr,_SIZE_B2H_BLK

	cpltu	t0,b2h_b_ptr,b2h_b_lim
	jmpti	t0,t4			;done DMA unless wrapped the ring
	 nop
	 ;otherwise fall into dob2h

;do a B2H transfer
;   entry:  t4=return
;   exit: t0-t3 changed
dob2h:
	sub	t3,b2h_b_ptr,b2h_b_base	;t3=DMA length
	DODMA	t3,B2H, b2h_b_base,0, 0,b2h_h_ptr,0, t0,t1
	add	b2h_h_ptr,b2h_h_ptr,t3	;advance pointer in host buffer

	set_st	DMA_CACHE		;remember to flush the DANG cache

	constx	t0,b2h_0 ^ b2h_1	;switch to other board buffer
	xor	b2h_b_base,b2h_b_base,t0    ;while this one cools

	add	b2h_b_lim,b2h_b_base,BD_B2H_SIZE
	add	b2h_b_ptr,b2h_b_base,0	;b2h_b_ptr=start of board buffer

	sub	t1,b2h_h_lim,b2h_h_ptr	;t1=bytes to end of host buffer
	cpge	t0,t1,BD_B2H_SIZE
	jmpti	t0,t4			;done if far from end of host buffer
	 add	b2h_b_lim,b2h_b_base,BD_B2H_SIZE

	cpneq	t0,t1,0			;t0=TRUE if at end of host buffer

	jmpti	t0,t4			;done if have space before end of
	 add	b2h_b_lim,b2h_b_base,t1	;   host buffer

    ;wrap to start of host buffer at its end
	lod	b2h_h_ptr,b2h_h_base
	jmpi	t4
	 add	b2h_b_lim,b2h_b_base,BD_B2H_SIZE


;Tell host about finished output.
;   We must already know there is something to talk about.
;   entry:  v0=return
;   exit: v1,v2,t0-t4 changed
host_fin_out:
	add	v2,p_spriv0,(&out_done-SREG0)*4

	sll	t1,sn,_SIZE_SGRAM_BT
	add	t1,t1,v2
	store	0,WORD,out_done,t1	;home count for current SONIC

    ;Scan all of the SONICs starting from #1, looking for output that
    ;needs to be reported to the host.  We know at least one has some.
	const	v1,0			;v1=SONIC being checked
host_fin_out1:
	sll	t1,v1,_SIZE_SGRAM_BT
	add	t1,t1,v2		;t1=pointer to out_done for a SONIC
	load	0,WORD,out_done,t1

	add	t0,v1,B2H_ODONE>>B2H_PORT0_BT
	sll	t0,t0,B2H_PORT0_BT	;t0=op:port

	cpeq	t4,out_done,0		;t4=0 if something finished
	jmpt	t4,host_fin_out6	;skip this SONIC if nothing finished

	 add	t0,t0,out_done		;t0=combined count and opcode
	const	out_done,0
	store	0,WORD,out_done,t1	;clear the count

	call	t4,addb2h		;schedule transmission to the host
	 set_st	OUTINT_REQ

host_fin_out6:
	cplt	t0,v1,EP_PORTS-1
	jmpt	t0,host_fin_out1
	 add	v1,v1,1			;next SONIC

	rs_st	OUT_DONE

	cpneq	t0,b2h_b_ptr,b2h_b_base
	jmpt	t0,dob2h		;send to host
	 add	t4,v0,0			;   set t4=our return

	jmpi	t4			;unless board-to-host buffer already
	 nop				;emptied, probably by addb2h




;Skim input packets, discarding wrong multicast or promiscuous addresses.
;   We know at least one complete packet is ready and there are
;	enough mbufs for it.
;   If there has been a babble packet recently, and the next RRA is not
;	the buffer in the next RDA, then process a babble packet.
;   Otherwise, process normal a packet.
rx:
    ;We almost always need the contents of the receive descriptor (except
    ;for the sequence numbers), and the penalty for getting an extra word
    ;is one clock during the loadm but there are many wait states to load
    ;a single word, so get all of the descriptor now.
	mtsrim	CR,_SIZE_RXpkt/4-1
	loadm	0,WORD,v0,rda
	mtsrim	BP,0			;v0=untrimmed _RXpkt_status,
					;v1=untrimmed _RXpkt_size,
	inhw	v2,v2,v3		;v2=buffer address
					;v6=_RXpkt_inuse

	tbit	t0,v6,RX_PTK_INUSE_BUSY	;t0=false if next descriptor finished
	jmpt	t0,rx_done		;quit if no more finished packets

	 cpeq	t0,v2,rbuf
	jmpf	t0,rx_babble		;process a babble packet

	 sub	v1,v1,ETHER_CRC_SIZE	;v1=net packet size
	consth	v1,0

    ;The SONIC will not accept packets that are not fragments but do have
    ;CRC errors.
	and	v0,v0,_RXpkt_mask	;v0=trimmed status bits
	tbit	t0,v0,RCR_COL
	jmpt	t0,rx10			;t0=only count known collisions

	 cplt	t0,v1,MIN_ETHER		;is it a runt?
	jmpf	t0,rx20
	 and	t0,v0,RCR_CRCR|RCR_FAER
	cpeq	t0,t0,0			;yes, runts with CRC errors are
	jmpf	t0,rx20			;   collisions from beyond bridges.
	 nop

rx10:	UPCNT	rx_col,1,t0,t1		;count and discard collisions
	jmp	rx95
	 nop

;see if this packet matches the address filters
;   v0=packet status for host, v1=packet size, v2=SONIC buffer address
rx20:	load	0,WORD,v4,v2		;v4=4 bytes of DA
	t_st	t3,AMULTI
	sll	t0,v4,31-24		;t0=group bit from DA
	andn	t0,t0,t3		;t3=TRUE if multicast & not all-multi
	jmpf	t3,rx40			;do not check if want all multicasts

	 add	v5,v2,4
	load	0,HWORD,v5,v5		;v5=rest of the DA

	srl	v6,v4,16
	xor	v6,v6,v4
	xor	v6,v6,v5		;XOR 6 bytes of DA into 8 bits in v6
	srl	t1,v6,8
	xor	v6,v6,t1

	and	t1,v6,0xe0		;use 3 bits of hash to pick the word
	srl	t1,t1,5-2
	GRAMADR	t2,dahash,t3
	add	t1,t1,t2
	load	0,WORD,t3,t1		;t3=word in the hash table

	sll	t3,t3,v6
	jmpf	t3,rx_junk		;junk frame if hash table says no
	 nop


;We are committed to sending the packet to the host.
;   Copy the packet from SONIC SRAM to GIORAM and align and checksum it.
;   Checksum the entire thing including the error bits, and let
;	the host correct the value.
;   Put 16 error bits in front of the DA.

;   v0=packet status for host, v1=packet size, v2=SONIC buffer
rx40:
	.equ	N_RXSUM, N_SUMREG-1	;words/block checksummed

	add	v3,v2,0			;v3=pointer into SONIC buffer
	add	v4,v1,2			;(include 2 bytes of status)
	andn	v4,v4,3			;v4=bytes to copy in main loop
	add	v5,p_rbounce,RX_PAD-2	;v5=pointer into bounce buffer
	const	v6,0			;v6=checksum

	const	v10,(&sumreg1+N_RXSUM-1)*4  ;full blocks find odd bytes here
	mtsr	IPA,v10
	mtsrim	FC,16

rx45:	const	v8,N_RXSUM*4		;v8=bytes to move this time
	sub	v4,v4,v8		;v4=remainder or -excess past the end
	jmpf	v4,rx46
	 const0	v9,N_RXSUM-1		;v9=words-1 to move this time

    ;deal with last block of the packet
	add	v8,v8,v4		;v8=bytes to move this time
	cpeq	t0,v8,0			;t0=true if no more words to move
	jmpt	t0,rx50			;quit and deal with dribble bytes

	 sra	t0,v4,2			;t0= -excess words past the end
	add	v9,v9,t0		;v9=words-1 to move this time

	add	v10,v10,v4		;adjust pointer to odd bytes
	mtsr	IPA,v10

	const	t0,rx48+N_RXSUM*8
	consth	t0,rx48+N_RXSUM*8
	sll	t1,v8,1			;2 instructions or 8 bytes/data word
	sub	t0,t0,t1		;t0=middle of unrolled loop

    ;Position 2 bytes from previous loop or the status bits for merging
rx46:	add	sumreg,v0,0		;position previous odd bytes

	mtsr	CR,v9
	loadm	0,WORD,sumreg1,v3	;fetch block from SONIC buffer

	add	v3,v3,v8		;advance ptr
	jmpti	v4,t0			;jump into following unrolled loop
	 add	v0,gr0,0		;save these odd bytes for next time

rx48:					;shift and sum the block
	.set	smrg,&sumreg+N_RXSUM-1
	.rep	N_RXSUM
	  extract %%(smrg+1),%%smrg,%%(smrg+1)
	  addc	  v6,v6,%%(smrg+1)
	  .set	  smrg,smrg-1
	.endr
	addc	v6,v6,0

	mtsr	CR,v9
	storem	0,WORD,sumreg1,v5

	jmpf	v4,rx45
	 add	v5,v5,v8


;Deal with the last bytes of the packet.
;   FC=16,  v0=up to last 2 bytes fetched of packet,
;   v1=packet data length,
;   v2=start of SONIC buffer,
;   v3=pointer to last bytes in SONIC buffer,
;   v5=pointer to last bytes in bounce buffer, v6=checksum
rx50:
	subr	t0,v1,32-RX_PAD
	and	t0,t0,3			;t0=4-leftover bytes
	cpeq	t1,t0,0
	jmpt	t1,rx60			;done if packet ended on a half word

	 cpeq	t2,t0,1
	jmpf	t2,rx51
	 const	v8,0
	load	0,WORD,v8,v3
rx51:	extract	v0,v0,v8		;combine last 2 bytes with buffer
	store	0,WORD,v0,v5		;save them

	add	v6,v6,v0		;checksum them
	addc	v6,v6,0

;fold checksum into 16 bits, but do not bother to convert 0 to minus-0.
rx60:	srl	t0,v6,16
	consth	v6,0xffff0000
	add	v6,v6,t0
	addc	v6,v6,0

	store	0,WORD,v6,p_rbounce	;save the checksum

;Pick the size of the mbuf.
;   v0=packet status for host, v1=packet data length, v2=SONIC buffer
	add	v4,v1,RX_PAD+DMA_ROUND-1
	andn	v4,v4,DMA_ROUND-1	;v4=bytes to DMA to the host

	cpgt	t0,v4,sml_size
	jmpt	t0,rx65
	 add	v7,sn,B2H_IN_BIG>>B2H_PORT0_BT

	add	v7,sn,B2H_IN_SML>>B2H_PORT0_BT
	USEBUF	sml,v5,v6,rx80

rx65:	USEBUF	big,v5,v6,


;start DMA of the packet to the host
;   v0=packet status for host, v1=packet data length, v2=SONIC buffer,
;   v6:v5=mbuf, v4=bytes to DMA to the host
;   v7=op:port for the host
rx80:
	DODMA	v4,B2H, p_rbounce,0, v6,v5,0, t0,t1

	const	t0, rbounce0 ^ rbounce1
	xor	p_rbounce,p_rbounce,t0	;toggle p_bounce

	sll	v7,v7,B2H_PORT0_BT
	call	t4,addb2h		;tell host
	 add	t0,v7,v1		;   t0=sn:op:length

    ;host will hear of old output and will think we are awake
	andn	g_st,g_st, GST_OUTINT_REQ | GST_H2B_SLEEP

	mfsr	h2b_atime,TMC
	cplt	t0,h2b_delay,h2b_atime	;is the H2B DMA awake?
	jmpf	t0,rx90
	 sub	h2b_atime,h2b_atime,h2b_sleep	;keep H2B DMA going
	consth	h2b_delay,TIME_NOW	;no, immediately do H2B DMA

;release the receive descriptor
;   v2=SONIC buffer,
rx90:
	cpeq	t0,v2,rbuf
	jmpf	t0,rx98			;babble frames have no descriptor

rx95:	 add	t0,rda,_RXpkt_inuse
	const	t1,RX_PTK_INUSE_BUSY
	store	0,WORD,t1,t0		;set in-use field of this descriptor

    ;Set EOL bit in this descriptor, and then reset it in the previous
    ;descriptor.  Do it in this order to keep the SONIC sane.
	add	t0,rda,_RXpkt_link
	load	0,WORD,t1,t0
	or	t2,t1,RX_EOL
	store	0,WORD,t2,t0		;set this EOL bit
	store	0,WORD,rda,rx_link	;reset previous EOL bit

	add	rx_link,t0,0		;save pointer to this EOL bit
	add	rda,t1,0		;advance to the next descriptor


;release the receive buffer area
rx98:
	SNADR	t0,RWP
	store	0,HWORD,rrp,t0		;SONIC can reuse resource area

	SRAMADR	t1,_RXrsrc_last,t0

	add	rrp,rrp,_SIZE_RXrsrc
	cpgt	t0,rrp,t1
	jmpf	t0,mlop_sonic
	 load	0,WORD,rbuf,rrp

	SRAMADR	rrp,_RXrsrc,t0		;wrap to start of receive buffer area
	jmp	mlop_sonic
	 load	0,WORD,rbuf,rrp


;stop looking for input packets to finish
rx_done:
	const	t0,ISR_PKTRX
	andn	isr,isr,t0

    ;If the receiver has shut down because of problems,
    ;then reset the error bits and turn the receiver back on.
	and	v0,isr,ISR_RDE|ISR_RBE
	cpeq	t2,v0,0
	jmpt	t2,mlop_sonic
	 andn	isr,isr,v0

	store	0,HWORD,v0,p_isr

    ;count receive buffers exhausted
	BITCNT	rx_rbe,ISR_RBE,v0,,t1,t2

    ;count receive descriptors exhausted
	tbit	t0,v0,ISR_RDE
	jmpf	t0,mlop_sonic
	 INCCNT	rx_rde,1,t1,t2
	SNADR	t0,CR
	constx	t1,CR_RXEN
	jmp	mlop_sonic
	 store	0,HWORD,t1,t0


;forget junk packets, those that do not match the hash table
rx_junk:
	INCCNT	rx_hash,1,t0,t1
	jmp	rx90
	 nop


;send babble frames to the host
;   Come here when the descriptor in the next buffer is unexpected.
;   That means either that there is no more output and we should stop,
;   or that there is a babble frame.
;
;   The SONIC does not allocate a descriptor for packets that are too big,
;   but it does allocate a buffer.  So if the buffer pointer in the
;   next RDA is not what it should be, assume the SONIC has allocated a
;   buffer for a babble frame and pretend there was a descriptor.
;
;   This happens very rarely, so do not optimize it.
rx_babble:
	INCCNT	rx_babble,1,t0,t1

	const	v0,0			;v0=status for the host
	const	v1,RPKT_SIZE		;v1=bogus size
	add	v2,rbuf,0		;v2=buffer address
	jmp	rx20
	 const	rbuf,0			;rbuf=0 to indicate babble packet




;Start DMA'ing the next output frame from the host to the board.
;   We know there is enough buffer space.
;   d2b_len=packet byte count, d2b_head=rest of job
tx:
	add	v12,d2b_head, _H2B_S2
	load	0,WORD,v12,v12		;v12=offsum:cksum

	add	d2b_head,d2b_head,_SIZE_H2B_BLK
	sub	tx_lim,tx_lim,1

	load	0,WORD,v9,d2b_head	;v9=1st host buffer len & EBus addr

 .ifdef DEBUGDMA
	 constx	t0,_SIZE_tbounce
	cpgtu	t0,d2b_len,t0
	jmpt	t0,tx_dma_bug
 .endif

	rs_st	DMA_CACHE		;DMA reads force the DANG cache

;DMA the parts of the packet to a bounce buffer
;   v12=offsum:cksum, d2b_len=packet byte count
	constx	v10,tbounce		;v10=initial board address for DMA
tx20:
	add	v1,d2b_head,_H2B_S2
	load	0,WORD,v1,v1		;v1=next host buffer address
	srl	v11,v9,H2B_ADDR_HI_LSB_BT
	and	v11,v11,H2B_ADDR_HI_MASK    ;v11=E-bus address bits
	consth	v9,0
	add	v9,v9,DMA_ROUND-1
	andn	v9,v9,DMA_ROUND-1	;v9=byte count
	CK	H2B_LEN_MSB_BT==15

	DODMA	v9,H2B, v10,0, v11,v1,0, t0,t1

	add	v10,v10,v9		;advance pointer into bounce buffer

	add	d2b_head,d2b_head,_SIZE_H2B_BLK
	load	0,WORD,v9,d2b_head	;v9=next host buffer length
	jmpf	v9,tx20			;quit at the end of string of buffers
	 CK	D2B_START == TRUE

;See if the packet is too short, and pad it if necessary.
;   v9=start of next host request, v12=offsum:cksum, d2b_len=packet byte count
	 subr	t0,d2b_len,MIN_ETHER-1	;t0=bytes to clear-1
	jmpt	t0,tx38
	 add	v3,d2b_len,0		;v3=byte count for SONIC

    ;Instead of clearing registers, we could loadm from a block that
    ;is know to be 0.  Fetching zeros from SRAM costs 6 leading wait states.
    ;Most short packets are TCP ACKs and so about 54 bytes long, needing
    ;only 3 words of zeros, and so the cost of zeroing the right number
    ;of registers is worthwhile.
	.equ	NZMIN, (MIN_ETHER+3)/4

	srl	t1,t0,2
	mtsr	CR,t1			;set words-1 to clear

	andn	t2,t0,3
	constx	t1,tx35+(NZMIN-1)*4
	sub	t1,t1,t2
	jmpi	t1
	 const	sumreg,0
tx35:					;zero some registers for the clearing
	.set	zrg,&sumreg+NZMIN-1
	.rep	NZMIN-1
	  const	%%zrg,0
	  .set	zrg,zrg-1
	.endr

	add	t1,d2b_len,_SIZE_TXpkt+TX_PAD
	add	t1,t1,obf
	andn	t1,t1,3			;t1=first word needing clearing

	storem	0,WORD,sumreg,t1	;clear the end of the buffer

	const	v3,MIN_ETHER		;v3=byte count for SONIC
tx38:

;allocate TX buffer
;   v3=byte count for SONCI, v9=start of next host request, v12=offsum:cksum,
;   d2b_len=packet byte count
	mov	v11,obf			;v11=start of TX descriptor

	add	v10,v3,_SIZE_TXpkt+TX_PAD+3
	andn	v10,v10,3		;v10=bytes of buffer actually used

	add	obf,obf,v10
	cpge	t0,obf,obf_end
	jmpf	t0,tx40
	 sub	obf_avail,obf_avail,v10

    ;Wrap to start after last buffer, and account for the unused space
    ;at the end of the buffer.
	SRAMADR	t0,_TXbufs+_SIZE_TXbufs,t2
	sub	t0,t0,obf
	sub	obf_avail,obf_avail,t0
	SRAMADR	obf,_TXbufs,t0


;create TX descriptor in v1-v8 and install it
;   Set the EOL bit just in case the SONIC is inactive.

;   v3=byte count for SONIC,
;   v9=start of next host request, v11=TX descriptor, v12=offsum:cksum,
;   d2b_len=packet byte count
tx40:	const	v1,0			;_TXpkt_status

					;_TXpkt_config
	add	sn_st,sn_st,SNST_PINT0
	;at least 1 interrupt per full-SRAM load to stay alive
	CK	SNST_PINT_OVF/SNST_PINT0<_SIZE_TXbufs/(ETHER_SIZE+_SIZE_TXpkt)
	and	v2,sn_st,SNST_PINT_OVF
	andn	sn_st,sn_st,v2
	mv_bt	v2,TCR_PINTR, v2,SNST_PINT_OVF

	;				v3=_TXpkt_psize from above

	const	v4,1			;_TXpkt_frags
	add	v5,v11,_SIZE_TXpkt+TX_PAD   ;_TXpkt_ptr_lo
	srl	v6,v5,16		;_TXpkt_ptr_hi
	add	v7,v3,0			;_TXpkt_fsize
	const	v8,TX_EOL
	add	v8,v8,obf		;_TXpkt_link
	CK	&v8==&v1+(_SIZE_TXpkt-4)/4
	mtsrim	CR,_SIZE_TXpkt/4-1
	storem	0,WORD,v1,v11

;copy and checksum from the bounce buffer to SONIC SRAM
	.equ	N_TXSUM, N_SUMREG-1	;words/block checksummed

    ;offsum:cksum == 1 means no checksumming is wanted
	cpeq	v1,v12,1		;v1=TRUE if no checksumming,

	WAITDMA	t0,fast

	 add	v3,v11,_SIZE_TXpkt	;v3=pointer into SONIC buffer
	add	v4,d2b_len,TX_PAD
	andn	v4,v4,3			;v4=bytes to copy in main loop
	constx	v5,tbounce		;v5=pointer into bounce buffer
	add	v6,v12,0
	consth	v6,0			;v6=checksum
tx45:	const	v8,N_TXSUM*4		;v8=bytes to move this time
	sub	v4,v4,v8		;v4=remainder or -excess past the end
	jmpf	v4,tx46
	 const0	v2,N_TXSUM-1		;v2=words-1 to move this time

    ;deal with last block in the packet
	add	v8,v8,v4		;v8=bytes to move this time
	cpeq	t0,v8,0			;t0=true if no more words to move
	jmpt	t0,tx50			;quit and deal with dribble bytes

	 sra	v4,v4,2			;v4= -excess words past the end
	add	v2,v2,v4		;v2=words-1 to move this time

	const	t0,tx48+N_TXSUM*4
	consth	t0,tx48+N_TXSUM*4
	sub	t0,t0,v8		;t0=middle of unrolled loop

tx46:	mtsr	CR,v2
	loadm	0,WORD,sumreg,v5	;fetch a block from bounce buffer

	jmpt	v1,tx49			;do not sum bytes if useless
	 add	v5,v5,v8		;advance pointer to source
	jmpti	v4,t0
	 add	v6,v6,sumreg		;start to sum and clear the carry bit
tx48:					;shift and sum the block
	.set	smrg,&sumreg+N_TXSUM-1
	.rep	N_TXSUM-1
	  addc	  v6,v6,%%smrg
	  .set	  smrg,smrg-1
	.endr
	addc	v6,v6,0			;get the last carry bit

tx49:	mtsr	CR,v2
	storem	0,WORD,sumreg,v3	;store the block into SONIC buffer

	jmpf	v4,tx45
	 add	v3,v3,v8		;advance SONIC buffer pointer


;deal with the last bytes of the packet
;   v1=TRUE if no checksumming,
;   v3=pointer into bounce buffer at last bytes of the packet,
;   v5=pointer at last bytes in SONIC buffer, v6=partial checksum,
;   v9=start of next host request, v11=TX descriptor, v12=offsum:cksum,
;   d2b_len=packet byte count
tx50:	subr	t0,d2b_len,32-TX_PAD
	and	t0,t0,3			;t0=4-leftover bytes
	cpeq	t1,t0,0
	jmpt	t1,tx60			;done if the packet ended on a word

	 sll	t0,t0,3			;t0=shift count to clean leftovers
	load	0,WORD,v0,v5
	srl	v0,v0,t0
	sll	v0,v0,t0
	store	0,WORD,v0,v3		;clean and copy the dribble bytes

	add	v6,v6,v0		;checksum them
	addc	v6,v6,0

;install checksum if allowed
;   v1=TRUE if no checksumming,
;   v9=start of next host request,
;   v6=complete checksum but not folded or complemented,
;   v11=TX descriptor, v12=offsum:cksum
tx60:	jmpt	v1,tx70			;do not checksum frame if not wanted

	 srl	v7,v12,16
	add	v7,v7,v11
	add	v7,v7,_SIZE_TXpkt
	andn	v8,v7,3			;v8=destination of checksum
	load	0,WORD,v4,v8		;v4=word containing checksum
	and	v7,v7,3			;v7=position in the word

	srl	t0,v6,16
	consth	v6,0xffff0000
	add	v6,v6,t0
	addc	v6,v6,0			;fold into 16 bits
	nor	v6,v6,0			;1's complement

	consth	v6,0			;v6=checksum folded into 16 bits
	cpeq	t0,v6,0			;convert +0 to -0 for UDP
	sra	t0,t0,31
	or	v6,v6,t0

	mtsr	BP,v7
	inhw	v4,v4,v6
	store	0,WORD,v4,v8		;install the checksum

tx70:	store	0,WORD,v11,tx_link	;give descriptor to the SONIC
	add	tx_link,v11,_TXpkt_link

	add	out_done,out_done,1	;as far as the host cares, we are done
	set_st	OUT_DONE

;   v9=start of next host request, d2b_head=address of start of next request
	cpge	v8,d2b_head,d2b_lim
	jmpf	v8,tx85
	 CK	((D2B_END&0xffff)==0) && (H2B_LEN_MSB_BT <= 15)
	 consth	v8,~(D2B_END | (H2B_LEN_MSB*2-1))
	CK	D2B_START == TRUE
	GRAMADR	d2b_head,d2b,t0		;wrap to start of host-to-board queue
	load	0,WORD,v9,d2b_head
tx85:
    ;set d2b_len=length of next job or D2B_END, a length greater
    ;than the maximum possible available free output buffer space.
    ;In the latter case, we will not have enough space and so will
    ;will not try to start the next output packet, which is good
    ;since D2B_END means there is not yet a next packet.
	andn	d2b_len,v9,v8

    ;if the SONIC is asleep, make a note to awaken it.
	t_st	t0,TX_ACT
	jmpt	t0,mlop_sonic
	 const	t0,ISR_PINT
	jmp	mlop_sonic
	 or	isr,isr,t0

 .ifdef DEBUGDMA
;stop on impossible length
tx_dma_bug:
	mtsrim	CPS,PS_INTSOFF
	jmp	.
	 halt
	nop
 .endif


;note when the SONIC says it has stopped
tx_stop:
	rs_st	TX_ACT
	const	t0,ISR_TXDN
	andn	isr,isr,t0

;see about finished output
tx_fin:
	const	t1,ISR_PINT
	andn	isr,isr,t1

    ;scan the transmit descriptors, processing finished buffers
tx_fin1:
	cpge	t0,obf_avail,max_obf_avail
	jmpt	t0,tx_start		;check only if unfinished

	 mtsrim	CR,_TXpkt_psize/4+1-1	;v1=status
	loadm	0,WORD,v1,tx_head	;v3=length
	CK	&v3==&v1+_TXpkt_psize/4

	cpeq	t0,v1,0			;stop on a zero status word
	jmpt	t0,tx_start

	 andn	v2,v1,TCR_PTX		;v2=error bits
	cpeq	t0,v2,0
	jmpt	t0,tx_fin3		;common case--skip error counting

	 srl	t0,v1,TX_COL_BT
	and	t0,t0,TX_COL_MASK
	INCCNT	tx_col,t0,t1,t2		;count collisions

	BITCNT	tx_def,TCR_DEF,v1,,t1,t2

	const	t0,TCR_EXD|TCR_CRSL|TCR_EXC|TCR_OWC|TCR_PMB|TCR_FU|TCR_BCM
	and	t0,t0,v1
	cpeq	t1,t0,0
	jmpt	t1,tx_fin3
	 UPCNT	tx_error,1,t1,t2

    ;count inidividual errors
	BITCNT	tx_exd,TCR_EXD,v1,tx_fin_exd,t1,t2
	rs_st	TX_ACT			;excessive deferrals stop transmitter
	set_st	TX_SICK
tx_fin_exd:
	BITCNT	tx_crsl,TCR_CRSL,v1,,t1,t2
	BITCNT	tx_exc,TCR_EXC,v1,tx_fin_exc,t1,t2
	rs_st	TX_ACT			;excessive collisions stop transmitter
	set_st	TX_SICK
	andn	v1,v1,TCR_BCM		;excessive collisions often cause BCM
tx_fin_exc:
	BITCNT	tx_owc,TCR_OWC,v1,,t1,t2
	BITCNT	tx_pmb,TCR_PMB,v1,,t1,t2
	BITCNT	tx_fu,TCR_FU,v1,tx_fin_fu,t1,t2
	rs_st	TX_ACT			;FIFO underrun stops transmitter
	set_st	TX_SICK
tx_fin_fu:
	BITCNT	tx_bcm,TCR_BCM,v1,,t1,t2
	rs_st	TX_ACT			;byte count mismatch stops transmitter
	set_st	TX_SICK
tx_fin_bcm:


tx_fin3:
	add	v3,v3,_SIZE_TXpkt+TX_PAD+3
	andn	v3,v3,3			;v3=length including descriptor & pad

	add	tx_head,tx_head,v3
	cpge	t0,tx_head,obf_end
	jmpf	t0,tx_fin1
	 add	obf_avail,obf_avail,v3

    ;wrap to start after last buffer, and account for the used space at
    ;at the end of the buffer.
	SRAMADR	t0,_TXbufs+_SIZE_TXbufs,t2
	sub	t0,t0,tx_head
	SRAMADR tx_head,_TXbufs,t1
	jmp	tx_fin1
	 add	obf_avail,obf_avail,t0

;Turn on transmitter if any work is waiting
;   This can be necessary if an error shut down the transmitter.
;   The transmitter must not be turned on unless the SONIC CAM has finished
;   loading the CAM.
tx_start:
	t_st	t0,TX_ACT
	jmpt	t0,mlop_sonic		;do not if SONIC is already busy

	 cpeq	t1,obf,tx_head
	jmpt	t1,mlop_sonic		;do not if nothing to do

	 t_st	t2,LCD_WAIT
	jmpt	t2,mlop_sonic

	 t_st	t0,TX_SICK		;if the transmitter got sick,
	jmpf	t0,tx_start5
	 SNADR	t0,CTDA
	store	0,HWORD,tx_head,t0	;then restore TDA

tx_start5:
	const	t0,CR_TXP
	SNADR	t1,CR
	store	0,HWORD,t0,t1
	jmp	mlop_sonic
	 set_st	TX_ACT




;process a host command
;   Interrupts are still off from the host-to-board interrupt handler.
cmd:
    ;see what the host wants
	DODMA	HC_CMD_RSIZE,H2B, p_host_cmd,hc_cmd, 0,host_cmd_buf,hc_cmd, t0,t1

	consth	g_st,0
	CK	GST_MB == TRUE

    ;get ready to acknowledge it
	constx	v4,cmd_id		;v4=address of cmd_id
	load	0,WORD,t2,v4

	rs_st	DMA_CACHE		;reads force the DANG cache

	WAITDMA	t0,fast

	 mtsrim	CR,3-1
	loadm	0,WORD,v0,p_host_cmd	;v0=hc_cmd, v1=hc_cmd_id, v2=data

	cpeq	t0,v1,t2
	jmpt	t0,mlop_cmd		;quit on spurious interrupt
	 CK	hc_cmd_id == hc_cmd+4

	cpltu	t3,v0,(cmd_tbl_end-cmd_tbl)/4
	jmpf	t3,cmd_error		;die on bad command

	 constx	t2,cmd_tbl
	sll	t4,v0,2
	add	t2,t2,t4
	load	0,WORD,t2,t2		;fetch function address

	STO_CMD v1,hc_cmd_ack,t0	;get ready to acknowledge it
	const	v0,HC_CMD_WRES_SIZE

    ;go to the function with v2=first word of data
	jmpi	t2
	 store	0,WORD,v1,v4		;save new cmd_id against spurious int


;Tell host the result of the command
;   v0=bytes to write to the host
cmd_done:
	add	v0,v0,hc_cmd_ack-hc_sign    ;also send the signature
	DODMA	v0,B2H, p_host_cmd,hc_sign, 0,host_cmd_buf,hc_sign, t0,t1

	jmp	mlop_cmd		;try to find an active SONIC
	 nop


;bad command from the host
cmd_error:
	constx	t0,LEDS
	constx	t1,LED_YELLOW | LED_GREEN1 | LED_GREEN2
	store	0,WORD,t1,t0
	mtsrim	CPS,PS_INTSOFF
	jmp	.
	 halt
	nop




;host commands
;   All of these commands are entered with v2=first data word.



;download
sto_cksum:.word	0

cmd_sto:
	lod	v4,sto_cksum		;v4=data checksum

	t_st	t0,DOWNING
	jmpt	t0,cmd_sto7

    ;if not already set for it, then copy EPROM to GIORAM
    ;offset by same value used by host, and start checksum
	 .set	sto_len, 16
	 constx	t0,EPROM
	constx	t1,EP_DOWN
	constx	t3,EPROM_SIZE/4/sto_len-2
	CK	(EPROM_SIZE % sto_len) == 0
cmd_sto2:
	mtsrim	CR,sto_len-1
	loadm	0,WORD,sumreg,t0
	add	t0,t0,4*sto_len
	mtsrim	CR,sto_len-1
	storem	0,WORD,sumreg,t1	;copy EPROM to GIORAM
	jmpfdec	t3,cmd_sto2
	 add	t1,t1,4*sto_len

	const	v4,0			;v4=initialized checksum
	set_st	DOWNING,t0

cmd_sto7:
	mtsrim	CR,EP_DWN_LEN+2-1
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


cmd_poke:
	mtsrim	CR,EP_DWN_LEN+2-1
	add	t0,p_host_cmd,hc_cmd_data
	loadm	0,WORD,sumreg,t0	;fetch address, count, & data
	const	v3,&sumreg2*4
	sub	sumreg1,sumreg1,2

cmd_poke8:
	mtsr	IPA,v3
	mtsr	IPB,v3
	add	v3,v3,4
	store	0,WORD,gr0,sumreg	;store and checksum the data
	add	v4,v4,gr0
	jmpfdec	sumreg1,cmd_poke8
	 add	sumreg,sumreg,4

	inv				;things may have changed

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
	DODMA	HC_CMD_WRES_SIZE,B2H, p_host_cmd,hc_cmd_ack, 0,host_cmd_buf,hc_cmd_ack, t0,t1

	add	v7,p_host_cmd,hc_cmd_data+8
	mtsrim	CR,8-1
	loadm	0,WORD,v0,v7		;go to code with 8 args
	CK	EP_DWN_LEN >= 8

	WAITDMA	t0

    ;start the downloaded code, with interrupts off
	jmpi	t4
	 mtsrim	CPS,PS_INTSOFF



;upload
;   v2=source address
cmd_fet:
	STOREGS	t0,,			;home registers so they can be fetched

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



;set overall board parameters
;   v2=initial word of request
cmd_init:
    ;many basic configuration values are changed, so turn off interrupts
	mtsrim	CPS,PS_INTSOFF

	.set	nxt,hc_cmd_data+4
 .macro	ghword, reg,addr,tmp,i1,i2
	  LOD_CMD reg,nxt,H,
	  .set	  nxt,nxt+2
	adj_word reg,addr,tmp,i1,i2,
 .endm

 .macro	gword,	reg,addr,tmp,i1,i2
	  LOD_CMD reg,nxt,,
	  .set	  nxt,nxt+4
	adj_word reg,addr,tmp,i1,i2,
 .endm


 .macro	adj_word,reg,addr,tmp,i1,i2
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

;Get a number of microseconds and convert to clock ticks.
;   ensure we are still running at 25 MHZ
	CK	MHZ>=(1<<MHZ_BT)*5/4 && MHZ<=(1<<MHZ_BT)*2
 .macro	gtime,	reg,addr,tmp
	gword	reg,addr,tmp,sll,MHZ_BT
	  srl	  tmp,reg,1
	  add	  reg,reg,tmp
 .endm

	sto	v2,b2h_h_base,t0	;position in host buffer
	mov	b2h_h_ptr,v2

    ;The host-to-board buffer length used by the board must be no larger
    ;than the actual buffer provided by the host, or the board will trash
    ;host memory.  If the board uses a smaller value, data transfers
    ;and the board will appear to the host to be quiet.
    ;The host buffer must be big enough, or the board will write over
    ;previous notes before the host has seen them.  We cannot refuse procede
    ;if the lengths differ, since that would prevent downloading new
    ;firmware.  To let the host know, we just refuse to acknowledge the
    ;command
	gword	v1
	constx	v2,B2H_LEN
	cpeq	t0,v1,v2
	jmpt	t0,cmd_init4

	 cplt	t0,v1,v2
	sra	t0,t0,31
	and	v1,v1,t0
	andn	v2,v2,t0
	or	v1,v1,v2
	STO_CMD	t0,hc_cmd_ack,t1
cmd_init4:

	sll	v1,v1,2
	CK	_SIZE_B2H_BLK == 4
	add	b2h_h_lim,b2h_h_ptr,v1

	gword	h2b_pos,h2b_base,v3

	gtime	h2b_dma_delay,,v3

	gtime	h2b_sleep,,v3

	gtime	int_delay,,v3

	ghword	v1,max_bufs,v3,sub,MIN_BIG+MIN_SML

	ghword	sml_size

	mtsrim	CPS,PS_INTSON
	jmp	cmd_done
	 const	v0,HC_CMD_WRES_SIZE

	.purgem	gword





;set the MAC multicast mode, address filters, and so on
;   v2=initial word of request, the port number
cmd_mac_parms:
	SELREGS	v2,t1

	const	v5,SONIC_RESET0
	sll	v5,v5,sn		;v5=reset & active bit for this SONIC
	or	snact,snact,v5		;mark SONIC active
	CK	SONIC_RESET0 == SNACT0
	add	v6,snreset,0		;v6=old value of reset register
	andn	snreset,snreset,v5
	sto	snreset,SONIC_RESET,t2	;turn off board SONIC reset

	cpeq	v6,v6,snreset		;v6=true if SONIC was already on

	add	v0,p_host_cmd,hc_cmd_data+4
	mtsrim	CR,MMODE_HLEN/4-1
	loadm	0,WORD,sumreg,v0
	GRAMADR	v0,dahash,t3
	mtsrim	CR,MMODE_HLEN/4-1	;set DA hash table
	storem	0,WORD,sumreg,v0

    ;copy MAC address to SONIC SRAM to later put it into the CAM
	add	v0,p_host_cmd,hc_cmd_data+((4+MMODE_HLEN+2) & ~3)
	mtsrim	CR,2-1
	loadm	0,WORD,v1,v0		;fetch v1,v2=MAC address
	SRAMADR	v0,_CAM,t0
	const	sumreg,0		;CAM entry pointer
	add	sumreg1,v1,0
	consth	v1,0			;CAM Address Port 0
	srl	sumreg2,v2,16		;CAM Address Port 1
	add	sumreg3,v2,0
	consth	sumreg3,0		;CAM Address Port 2
	const	sumreg4,1		;CAM enable
	mtsrim	CR,5-1
	storem	0,WORD,sumreg,v0

    ;set up everything if the chip was off
	jmpt	v6,cmd_mac_parms5
	 nop

    ;set up the private registers
	constx	d2b_len,D2B_END
	GRAMADR	d2b_head,d2b,t0
	add	d2b_tail,d2b_head,0
	SRAMADR	obf,_TXbufs,t0
	    ;Reduce apparent buffer space by overhead, padding, and
	    ;padding for small packets to simplify comparisons when looking
	    ;for room for new packets.
	add	obf_avail,max_obf_avail,0

	add	tx_head,obf,0
	add	tx_link,obf,obf_avail	;point tx_link to end of buffer

	    ;p_isr was set when the board was initialized

	    ;set limit at end of buffer one maximum sized packet before
	    ;the end to simplify things
	constx	t0,_SIZE_TXbufs - (_SIZE_TXpkt+_SIZE_tbounce)
	add	obf_end,obf,t0
	GRAMADR	d2b_lim,d2b_end,t0
	STOREGS	t0,all,


    ;set SONIC data configuration registers
    ; DCR=bit 10=1 synchronous, 32 bit mode, FIFO thresholds at 16, half way
    ;	    EXBUS   0	TXC is used, and so cannot use STERM- pin
    ;	    14	    0
    ;	    LRB	    X	BRT is asserted
    ;	    PO	    X
    ;	    SBUS    0	Asynchronous
    ;	    USR	    X
    ;	    WC:	    00  No added bus cycles
    ;	    DW:	    1	32-bit mode
    ;	    BMS	    1	BLock mode - RFT and TFT number of bytes
    ;	    RFT	    10  4 long words (16 bytes)
    ;	    TFT	    10  4 long words (16 bytes)
	constx	t0,0x3a
	SNADR	t1,DCR
	store	0,HWORD,t0,t1

	constx	t0,0
	SNADR	t1,DCR2
	store	0,HWORD,t0,t1

	SNADR	v0,CR			;v0=SONIC command register

	const	t0,0
	store	0,HWORD,t0,v0		;turn off SONIC software reset bit

	SRAMADR	t1,_TXbufs,t0		;set up output
	SNADR	t0,UTDA
	srl	t2,t1,16
	store	0,HWORD,t2,t0
	SNADR	t0,CTDA
	store	0,HWORD,t1,t0

	SRAMADR	rrp,_RXrsrc,t0		;set up input
	SNADR	t0,URRA
	srl	t1,rrp,16
	store	0,HWORD,t1,t0
	SNADR	t0,URDA
	store	0,HWORD,t1,t0
	SNADR	t0,RSA
	store	0,HWORD,rrp,t0
	SNADR	t0,RRP
	store	0,HWORD,rrp,t0
	SNADR	t0,RWP
	SRAMADR	t1,_RXrsrc_last,t2
	store	0,HWORD,t1,t0
	SNADR	t0,EOBC
	const	t1,(RBUF_SIZE-4)/2
	store	0,HWORD,t1,t0

	SNADR	v1,CDC
	const	t0,1
	store	0,HWORD,t0,v1
	SNADR	v1,CDP
	SRAMADR	t0,_CAM
	store	0,HWORD,t0,v1
	const	t0,CR_LCAM
	store	0,HWORD,t0,v0		;load the CAM
	set_st	LCD_WAIT,t0

	SRAMADR	v1,_RXbufs,t0
	add	rbuf,v1,0		;rbuf=first buffer that will be used
	const	v3,RBUF_SIZE/2
	const	v4,0
	add	v5,rrp,0
	const	v6,NUM_RX_BUFS-2
cmd_mac_parms1:
	mtsrim	CR,_SIZE_RXrsrc/4-1
	srl	v2,v1,16
	storem	0,WORD,v1,v5		;create a receive resource area
	add	v5,v5,4*4
	add	v1,v1,v3		;start to advance buffer address
	jmpfdec	v6,cmd_mac_parms1
	 add	v1,v1,v3		;finish advancing buffer address

	SNADR	t0,REA
	store	0,HWORD,v5,t0

    ;create receive descriptors
	SRAMADR	rda,_RXpkt
	SNADR	t0,CRDA
	store	0,HWORD,rda,t0
	mtsrim	CR,_SIZE_RXpkt/4-1
	constx	t0,zeros
	loadm	0,WORD,sumreg,t0
	const	sumreg6,RX_PTK_INUSE_BUSY
	add	v5,rda,0
	constx	v6,NUM_RX_BUFS-2-1	;loop over all but the the last one
cmd_mac_parms2:
	add	sumreg5,v5,_SIZE_RXpkt	;link this descriptor to the next
	mtsrim	CR,_SIZE_RXpkt/4-1
	storem	0,WORD,sumreg,v5	;store it
	jmpfdec	v6,cmd_mac_parms2
	 add	v5,sumreg5,0
	const	sumreg5,RX_EOL		;last descriptor has EOL bit
	add	sumreg5,sumreg5,rda	;   and points to the first one
	mtsrim	CR,_SIZE_RXpkt/4-1
	storem	0,WORD,sumreg,v5
	add	rx_link,v5,_RXpkt_link	;the last descriptor is "previous"


    ;clear counters
	constx	t0,zeros
	mtsrim	CR,_SIZE_cnts0/4-1
	CK	_SIZE_cnts0/4 <= N_SUMREG
	loadm	0,WORD,sumreg,t0
	CNTADR	t0,all
	mtsrim	CR,_SIZE_cnts0/4-1
	storem	0,WORD,sumreg,t0

	SNADR	v1,IMR
	const	t0,IMR_BITS
	store	0,HWORD,t0,v1		;turn on interesting SONIC interrupts

	const	t0,CR_RXEN | CR_RRRA
	store	0,HWORD,t0,v0		;enable RX

	SNADR	v1,MPT
	load	0,HWORD,v2,v1
	GRAMADR	v1,old_mpt,t0
	store	0,WORD,v2,v1		;snapshot tally counters

cmd_mac_parms5:
	add	v4,p_host_cmd,hc_cmd_data+4+MMODE_HLEN
	load	0,HWORD,v4,v4		;v4=mode bits from host
	rs_st	AMULTI
	and	v3,v4,MMODE_AMULTI	;v3=all-multicast bit
	mv_bt	t0,SNST_AMULTI, v3,MMODE_AMULTI
	or	sn_st,sn_st,t0		;remember all-multicast mode

    ;turn promiscuous mode on or off in the SONIC and set the other RCR bits
	const	t0, RCR_PRO
	mv_bt	v2,RCR_PRO, v4,MMODE_PROM
	and	v2,v2,t0
	const	t1, RCR_ERR | RCR_RNT | RCR_BRD | RCR_AMC
	or	v2,v2,t1
	SNADR	t2,RCR
	store	0,HWORD,v2,t2

	jmp	cmd_done
	 const	v0,HC_CMD_WRES_SIZE



;turn off a SONIC
;   v2=initial word of request, the port number
cmd_mac_off:
	SELREGS	v2,t1
	mtsrim	CPS,PS_INTSOFF

	const	v5,SONIC_RESET0
	sll	v5,v5,sn		;v5=reset bit for this SONIC
	or	snreset,snreset,v5

	sto	snreset,SONIC_RESET,t2	;reset the SONIC
	andn	gsor,gsor,snreset
	CK	GSOR_SIR0 == SONIC_RESET0

	andn	snact,snact,v5		;mark SONIC inactive
	CK	SNACT0_BT == SONIC_RESET0_BT

	const	out_done,0		;host has forgotten its pending output
	const	isr,0
	constn	obf_avail,-1
	constx	d2b_len,D2B_END
	constx	sn_st,0

	const	v0,HC_CMD_WRES_SIZE	;answer host with this many bytes
	jmp	cmd_done
	 mtsrim	CPS,PS_INTSON



;fetch counters
;   v2=host address to write the counters
cmd_fet_cnt:

	const	v0,0			;loop through all of the SONICs
cmd_fet_cnt2:
	subr	t0,v0,31-SONIC_RESET0_BT
	sll	t0,snreset,t0
	jmpt	t0,cmd_fet_cnt4		;if the SONIC is off, then go fast

	 SELREGS v0,t0

    ;update counts for this SONIC from tallies
	SNADR	v4,MPT			;v4=address of SONIC counter
	load	0,HWORD,v5,v4		;v5=current tally
	GRAMADR	v6,old_mpt,t0		;v6=address of old value

	load	0,HWORD,v7,p_isr	;v7=current ISR bits
	or	v7,v7,isr
	and	v7,v7,ISR_MP
	cpeq	t0,v7,0
	jmpt	t0,cmd_fet_cnt3		;be consistent if tally is rolling
	 andn	isr,isr,v7

	store	0,HWORD,v7,p_isr	;reset tally-rolled interrupt

	load	0,HWORD,v5,v4		;re-fetch tally to be consistent
	consth	v5,0x10000		;v5=effective new value
	const	t0,ISR_MP

cmd_fet_cnt3:
	load	0,WORD,v8,v6		;v8=old value
	sub	v8,v5,v8		;v8=new value - old value
	UPCNT	rx_mp,v8,t0,t1		;update the counter

	consth	v5,0
	store	0,WORD,v5,v6		;save the new old value

cmd_fet_cnt4:
	cpeq	t0,v0,EP_PORTS-1
	jmpf	t0,cmd_fet_cnt2
	 add	v0,v0,1

    ;On a new enough board, send the LINK bits to the host.
    ;On an old board, send ones.
    ;Use LINK counter for SONIC 0 for all of the bits.
    ;
	SELREGS	0,t0
	load	0,WORD,t0,p_gsor
	constx	v1,LINKS
	srl	t0,t0,GSOR_REV_BT
	and	t0,t0,GSOR_REV_MASK>>GSOR_REV_BT
	cpge	t1,t0,LINK_REV
	jmpf	t1,cmd_fet_cnt6
	 const	v3,0xff
    ;fetch the bits 4 times (thanks to the register being in the EEPROM
    ;part of the address space) and vote among the bits to work around
    ;noise in the register.
	load	0,WORD,v3,v1
	srl	t0,v3,16
	and	v3,v3,t0
	srl	t0,v3,8
	and	v3,v3,t0
	xor	v3,v3,0xff
 .ifdef DEBUGLINK
	cpeq	t0,v3,0xff
	jmpt	t0,cmd_fet_cnt6
	 nop
	nop
 .endif
cmd_fet_cnt6:
	STOCNT	v3,links,t2,


    ;send counters for all SONICs to the host
	DODMA	_SIZE_cnts,B2H, p_cnts,0, 0,v2,0, t0,t1
	jmp	cmd_done
	 const	v0,HC_CMD_WRES_SIZE



;refresh H2B DMA
cmd_wakeup:
	rs_st	H2B_SLEEP
	mfsr	h2b_atime,TMC		;keep H2B DMA active
	sub	h2b_atime,h2b_atime,h2b_sleep
	consth	h2b_delay,TIME_NOW	;see what the host has to say
	jmp	cmd_done
	 const	v0,HC_CMD_WRES_SIZE



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
	CNT_TIMER h2b_delay
	CNT_TIMER h2b_atime
	CNT_TIMER inth_time
	.purgem CNT_TIMER

	add	time_hz,time_hz,it2	;advance the clock
	iret
	 nop


;host to board interrupt
hostint:
	constx	it1,INTC_H2BINT
	store	0,WORD,it1,p_intc	;clear the interrupt
	load	0,WORD,it0,p_gsor	;ensure the hardware hears before iret

	consth	g_st,GST_MB		;tell the main loop to do the work
	CK	GST_MB == TRUE

	iret
	 nop


;bad interrupt
	.align	16
badint:
	constx	t0,LEDS
	constx	t1,LED_YELLOW | LED_GREEN0 | LED_GREEN1 | LED_GREEN2
	store	0,WORD,t1,t0
	mtsrim	CPS,PS_INTSOFF
	jmp	.
	 halt
	nop
	nop

    ;coff2firm assumes the last word is the version number
	.word	VERS

    ;there better be room for the checksum
	CK	. < GIORAM + EP_FIRM + EP_FIRM_SIZE - 4
	.end
