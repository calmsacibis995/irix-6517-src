;Common definitions for the EPROM ono the FDDIXPress, which combines the
;	dispersed bytes of the EPROM, and stores the resulting words
;	starting at 0

;This has to be small enough to fit in the 32-word PROM, but can be
;	as slow as convenient

;The most significant byte of the first word of the EPROM is expected to have 
;	the length of the contents of the EPROM, divided by 4096.


; Copyright 1990 Silicon Graphics, Inc.  All rights reserved.
	.ident "$Revision: 1.6 $"
	.file	"prom.s"

	.include "std.h"
	.include "hard.h"
	.include "eprom.h"

	.lflags	ns

	.sect	prom, text, absolute 0
	.use	prom

	.reg	src,	gr100		;EPROM source
	.reg	len,	gr99		;EPROM length
	.reg	tgt,	gr98		;SRAM target
	.reg	lcnt,	gr97		;loop count
	.reg	accum,	gr96		;build words here
	.reg	tmp,	gr95

	.equ	BEGIN,SRAM_BASE+0x400	;start executing here

	.global	start
start:	mtsrim	CPS,PS_INTSOFF | PS_RE | PS_FE	;get ready to start,
	mtsrim	OPS,PS_INTSOFF		;well before the iret instruction
	jmp	pcopy
	 nop				;keep the HALT out of the delay slot
	CK	. == 16
ac_fail:halt				;29K goes to "ROM 16" for Warn
	jmp	ac_fail


pcopy:	constx	tgt,SRAM_BASE		;destination in SRAM
	constx	src,EPROM_BASE+3	;source in ERPOM

	mtsrim	CFG, CFG_DW | CFG_VF
	mtsrim	PC1,BEGIN
	mtsrim	PC0,BEGIN+4

	load	0,BYTE,len,src		;figure out how far to go
	add	src,src,4
	sll	len,len,EPROM_SIZE_BT
	sub	len,len,2+1		;-2 for loop, -1 for 1st word of SRAM

stolop:	const	lcnt,4-2
fetlop:	load	0,BYTE,tmp,src		;load and combine an EPROM byte
	sll	accum,accum,8
	or	accum,accum,tmp

	jmpfdec	lcnt,fetlop		;store bunches of 4 bytes
	 add	src,src,4

	store	0,WORD,accum,tgt

	jmpfdec	len,stolop		;go to the code when done
	 add	tgt,tgt,4

	iretinv
	 nop				;not a delay slot but must be valid

	.end
