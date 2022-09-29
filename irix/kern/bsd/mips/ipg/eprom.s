;Contents of the "EPROM" on the FDDIXPress, which consists of
;	code to download the real "firmware" from the host.

; Copyright 1990 Silicon Graphics, Inc.  All rights reserved.

	.ident	"$Revision: 1.14 $"
	.file	"eprom.s"

	.include "std.h"
	.include "hard.h"
	.include "eprom.h"

	.set	CUR_NDELAY_LIM, MIN_NDELAY_LIM

;define the stacks
	.equ	MSTK_SIZE, 8*1024
	.equ	RSTK_SIZE, 8*1024

	.equ	mstk_base, SRAM_BASE + (SRAM_SIZE-MSTK_SIZE-RSTK_SIZE)
	.equ	mstk_top, mstk_base+MSTK_SIZE

	.equ	rstk_base, mstk_top
	.equ	rstk_init, rstk_base + RSTK_SIZE - 512
	.equ	rstk_top, rstk_base + RSTK_SIZE

	.if	rstk_top != (SRAM_BASE+SRAM_SIZE)
	.print	"memory allocation is wrong"
	.err
	.endif


;Traps and interrupts
	.sect	eprom,text,absolute SRAM_BASE
	.use	eprom

	.word	((EPROM_CODE_SIZE+EPROM_SIZE-1)>>EPROM_SIZE_BT) << 24

	nop				;HALT makes the emulator unhappy
	.rep	NUM_V - 2
	halt
	.endr


;Start running, but run only to download the real stuff
;	The PROM starts us running at 0x400.

;	The downloaded code must reset the FZ and DA bits and generally
;	initialize the universe.  If the stacks or trap handers here are
;	wrong, they must also be set in the downloaded code.

	.global	start
start:	mtsrim	CPS,PS_INTSOFF		;unfreeze
	mtsrim	VAB,0
	mtsrim	CFG,CFG_DW
	inv


clrs0:	const	lr1,0
	const	lr2,0
	const	lr3,0
	const	lr4,0
	.equ	zlen,(.-clrs0)/4
	const	v1,edata
	consth	v1,edata
	const	t2,(SRAM_SIZE-edata+SRAM_BASE)/4/zlen-2
	consth	t2,(SRAM_SIZE-edata+SRAM_BASE)/4/zlen-2
clrsram:mtsrim	CR,zlen
	storem	0,WORD,lr1,v1		;clear all of SRAM
	jmpfdec	t2,clrsram
	 add	v1,v1,zlen*4


;initialize the DRAM
	const	v0,0
	const	v1,DRAM_BASE
	consth	v1,DRAM_BASE
	const	v2,DMACS_BASE
	consth	v2,DMACS_BASE

;zap the DMACS register
;   dreg=register in the chip, val=to be stored, t0,t3=scratch,
;   v0=0, v1=DRAM start, v2=DMACS_BASE
	.set	DMACS_VAL, -1
 .macro	DMACS,	dreg, value
	  add	  t0,v2,dreg
	  store	  0,WORD,v0,t0
 .if	DMACS_VAL != value
	  .set	  DMACS_VAL, value
 .if value<=255
	  add	  t3,v1,value
 .else
	  constx  t3,value
	  add	  t3,t3,v1
 .endif
 .endif
	  load	  0,WORD,t0,t3
 .endm

	DMACS	0xa0,0x4560		;set edge triggered request
	NDELAY	t0,2000,0,		;wait for hardware to stablize
	DMACS	0xb0,0x10		;run in least latency mode
	DMACS	0x90,0xF0		;set short I/O to generate memory req
	DMACS	0x80,0xF0		;set cpu to generate requests
	DMACS	0x10,0xF0		;set front end cycles to generate
	DMACS	0x30,0xF0		;   memory requests
	DMACS	0x50,0xF0
	DMACS	0x70,0xF0

	constn	t0,-1			;reset "VRAM"
	constx	v1,VRAM_RESET
	store	0,WORD,t0,v1

	const	t0,BUSRST_WRITE
	const	v1,IO_BUSRST
	consth	v1,IO_BUSRST
	store	0,WORD,t0,v1


	constx	v1,DRAM_BASE
	const0	t2,BUS_PAGE/4/zlen-2
clrd1:	add	lr2,v1,0		;put a pattern every 16 bytes
	mtsrim	CR,zlen
	storem	0,WORD,lr1,v1
	jmpfdec	t2,clrd1
	 add	v1,v1,zlen*4

	constx	v1,DRAM_BASE
	constx	t1,XFER_CYCLE		;and then by clearing all pages
	or	v1,v1,t1
	load	0,WORD,t1,v1
	const0	t2,(DRAM_SIZE-BUS_PAGE)/BUS_PAGE-2
	const0	t1,BUS_PAGE
clrd2:	add	v1,v1,t1
	jmpfdec	t2,clrd2
	 store	0,WORD,v1,v1


	mtsrim	CR,255-64
	const	t1,edata
	consth	t1,edata
	loadm	0,WORD,gr64,t1		;clear the registers


	.reg	p_en1sel,v4
	.reg	p_en1reg,v5
	.reg	p_cmt1,	v6
	.reg	p_en2sel,v7
	.reg	p_en2reg,v8
	.reg	p_cmt2,	v9
	.reg	stat1,	v10
	.reg	p_cmd1,	v11

	constx	p_en1sel,IO_ENDEC1_SEL
	constx	p_en2sel,IO_ENDEC2_SEL
	xor	p_en1reg,p_en1sel,IO_ENDEC1_REG ^ IO_ENDEC1_SEL
	xor	p_en2reg,p_en2sel,IO_ENDEC2_REG ^ IO_ENDEC2_SEL
	constx	p_cmt1,IO_CMT1
	constx	p_cmt2,IO_CMT2
	constx	stat1, STAT1_CMT1 | STAT1_CMT2
	constx	p_cmd1, IO_CMD1

;set front end command register to turn on ENDEC R bus, reset the front
;   end, turn off the optical bypass, force a transmit abort, and
;   choose between the 2nd PHY and the other board for the repeating.
	load	0,HWORD,t0,p_cmd1
	CK	STAT1_MAC2_BT>CMD1_DATTACH_BT
	srl	t0,t0,STAT1_MAC2_BT-CMD1_DATTACH_BT
	const0	t1,CMD1_DATTACH
	and	t0,t0,t1
	const0	t1,CMD1_FCMS_END | CMD1_TXABT
	or	t0,t0,t1
	store	0,HWORD,t0,p_cmd1


;try to put the PHYs into a "repeat-mode" in case there is no optical
;   bypass switch
	const0	t0,ENDEC_CR2_RESET
	store	0,HWORD,t0,p_en1sel
	store	0,HWORD,t0,p_en2sel	;reset the ENDECs
	ENDEC_REC t1,ENDEC_WAIT+1,-PFETCH
	sll	t0,t0,ENDEC_SLL
	store	0,HWORD,t0,p_en1reg
	store	0,HWORD,t0,p_en2reg
	ENDEC_REC t1,ENDEC_WAIT+10,-PFETCH


	const	rfb,rstk_top		;set stack pointers
	consth	rfb,rstk_top
	const	rab,rstk_init
	consth	rab,rstk_init
	add	lr1,rfb,0
	sub	rsp,rfb,2*4		;make room for lr0 and lr1
	const	msp,mstk_top
	consth	msp,mstk_top


	.reg	dwn_ad,v0
	.reg	dwn_val,v1
	.reg	dwn_val2,v2

	constx	dwn_ad,SIO_DWN_AD
	xor	dwn_val,dwn_ad,SIO_DWN_VAL ^ SIO_DWN_AD
	add	dwn_val2,dwn_val,SIO_INCR
	const0	t0,EPROM_RDY
	store	0,HWORD,t0,dwn_ad



;See that all of SRAM was loaded correctly by computing a checksum.  This
;   is intended to catch a few bad bits in an largely correct EPROM,
;   and to be easy to adjust when changes are made to the source.
;The word cksum should be adjusted to make the checksum correct.

	const	t1,4			;check all but the 1st word
	const	t2,(edata-SRAM_BASE)/4-2
	consth	t2,(edata-SRAM_BASE)/4-2
	const	t3,0
sck:	load	0,WORD,t0,t1
	add	t1,t1,4
	add	t3,t3,t0
	srl	t0,t0,16
	jmpfdec	t2,sck
	 add	t3,t3,t0


	constx	t0,SIO_VERS		;tell host we are alive
	const	t1,1			;this is version #1
	store	0,HWORD,t1,t0

	xor	t0,t0,SIO_SIGN ^ SIO_VERS
	const0	t1,SGI_SIGN
	consth	t3,0
	add	t1,t1,t3
	store	0,HWORD,t1,t0

	load	0,WORD,t0,4
	store	0,WORD,t0,0		;fix EPROM size indication

	const	t1,cksum
	load	0,WORD,t2,t1		;adjust the checksum for the fix
	sub	t2,t2,t0
	srl	t0,t0,16
	sub	t2,t2,t0
	consth	t2,0
	store	0,WORD,t2,t1


	const0	t0,ENDEC_CR1_REPEAT	;get ENDECs ready to repeat
	store	0,HWORD,t0,p_en1sel
	store	0,HWORD,t0,p_en2sel
	ENDEC_REC t1,ENDEC_WAIT+1,-PFETCH
	sll	t0,t0,ENDEC_SLL
	store	0,HWORD,t0,p_en1reg
	store	0,HWORD,t0,p_en2reg
	ENDEC_REC t1,ENDEC_WAIT+4,-PFETCH


;Download from the host.  When there is nothing else to do, repeat
;	line states
dwnlod0:
	load	0,HWORD,t0,p_cmd1
	FE_DELAY 4,
	or	stat1,stat1,t0

 .macro CMTREP,	n
	  sll	t0,stat1,31-STAT1_CMT@n@_BT
	  jmpf	t0,dwnlod@n		;see if CMT state has changed
	   nop

	  load	0,BYTE,t2,p_cmt@n	;yes, get the new value
	  FE_DELAY 3,,
	  constx t0,STAT1_CMT@n
	  andn	stat1,stat1,t0
	  and	t2,t2,CMT_MASK
	  store	0,BYTE,t0,p_cmt@n	;reset CMT interrupt and latch
	  FE_DELAY 4,,

	  const	t0,ls2ls
	  add	t0,t0,t2		;convert to ENDEC value
	  add	t0,t0,t2
	  load	0,HWORD,t0,t0
	  store	0,HWORD,t0,p_en@n@sel
	  ENDEC_REC t1,1
	  sll	t0,t0,ENDEC_SLL
	  store	0,HWORD,t0,p_en@n@reg
	  ENDEC_REC t1,3
dwnlod@n:
 .endm
	CMTREP	1,			;repeat line states on 1st PHY

	sll	t0,stat1,31-STAT1_PHY2_BT
	jmpf	t0,dwnlod		;repeat on 2nd PHY if present
	 nop
	CMTREP	2,

	.purgem	CMTREP

dwnlod: load	0,HWORD,t0,dwn_ad	;get address/flag
	const	t1,EPROM_RDY
	cpeq	t1,t1,t0
	jmpt	t1,dwnlod0
	 nop

	load	0,HWORD,t1,dwn_val	;get and combine the value
	load	0,HWORD,t2,dwn_val2
	sll	t1,t1,16
	or	t1,t1,t2

	const	t2,EPROM_RDY		;tell host to get busy with the next
	store	0,HWORD,t2,dwn_ad

;here t0=address, t1=value, and the host has been told to continue
	const	t2,EPROM_GO
	cpeq	t2,t2,t0
	jmpti	t2,t1			;start executing when told
	 sll	t0,t0,2			;convert flag to probable address

	const	t2,edata
	consth	t2,edata
	cpge	t2,t2,t0
	jmpf	t2,dwnlod8		;ignore zeros stored on our code
	 cpeq	t2,t1,0
	jmpt	t2,dwnlod
	 nop

	const	t2,cksum
	load	0,WORD,t3,t2		;adjust the checksum for the change
	load	0,WORD,t4,t0
	add	t3,t3,t4
	srl	t4,t4,16
	add	t3,t3,t4
	consth	t3,0
	store	0,WORD,t3,t2


dwnlod8:jmp	dwnlod
	 store	0,WORD,t1,t0		;store the downloaded word



;convert ENDEC line state status to line state command
ls2ls:
	.hword	ENDEC_CR0_FILTER	;0 LSU
	.hword	ENDEC_CR0_FILTER	;1 NSU
	.hword	ENDEC_CR0_MLS		;2 MLS
	.hword	ENDEC_CR0_FILTER	;3 ILS

	.hword	ENDEC_CR0_HLS		;4 HLS
	.hword	ENDEC_CR0_QLS		;5 QLS
	.hword	ENDEC_CR0_FILTER	;6 ASU
	.hword	ENDEC_CR0_FILTER	;7 OVUF
	.align


cksum:	.word	(-0xc3c5) & 0xffff	;adjust to make the checksum right


	.align	zlen			;force alignment needed by zeroing
;everything after this is fodder
	.global	edata
edata:

	CK	. < EPROM_CODE_SIZE

	.end
