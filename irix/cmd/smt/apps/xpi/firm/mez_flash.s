;program FLASH EEROM on mezzanine FDDI card

; Copyright 1994 Silicon Graphics, Inc.  All rights reserved.
; "$Revision: 1.6 $"

	.file	"mez_flash.s"

	;XXX should be in giomez.h.
	;XXX needed in std.h, but giomez.h needs std.h
	.equ	AM29030,1

	.include "std.h"
	.include "gio.h"

	.include "if_xpi_s.h"

	.equ	VERS,((((VERS_Y-92)*13+VERS_M)*32+VERS_D)*24+VERS_H)*60+VERS_MM


	DEFBT	ROMW_A0,18		;ROM a0 for writes

    ;commands for the EEROM
	.equ	AM29_CYCLE_1,	0xaa000000
	.equ	AM29_CYCLE_2,	0x55000000
	.equ	AM29_RESET,	0xf0000000
	.equ	AM29_PGM,	0xa0000000
	.equ	AM29_SIG,	0x90000000
	.equ	AM29_ERASE,	0x80000000
	.equ	AM29_ERASE_CHIP,0x10000000

	.equ	A2AAA,		0x2aaa	;magic addresses
	.equ	A5555,		0x5555

	.equ	WH5555,	((A5555&3)<<ROMW_A0_BT) | (A5555 & ~3) | EPROM_WRITE
	.equ	WH2AAA,	((A2AAA&3)<<ROMW_A0_BT) | (A2AAA & ~3) | EPROM_WRITE

	DEFBT	DQ7, 7		;operation done
	DEFBT	DQ6, 6		;"toggle" bit
	DEFBT	DQ5, 5		;time limit exceeded
	DEFBT	DQ4, 4		;erase/(not programming) error bit
	DEFBT	DQ3, 3		;sector erase timer


;delay for a number of microseconds
 .macro	DELAY,	usec
  .if (usec)*(MHZ/2) > 0xffff
	  const	  tcnt,(usec)*(MHZ/2)
	  call	  tret,delay
	   consth tcnt,(usec)*(MHZ/2)
  .else
	  call	  tret,delay
	   const  tcnt,(usec)*(MHZ/2)
  .endif
 .endm


;do a command
;   Send the preamble, the command byte, and then flush the pipe to
;   ensure the chip heard.
 .macro	AM29,	cmd, tmp
	  store	  0,WORD,am29_cycle_1,wh5555
	  store	  0,WORD,am29_cycle_2,wh2aaa
	  constx  tmp,AM29_@cmd@
	  store	  0,WORD,tmp,wh5555
 .endm

;make a flash write pointer from a read pointer at the same location
 .macro	MAKWPTR, tgt,rp,tmp
	  and	  tmp,rp,3
	  andn	  tgt,rp,3
	  sll	  tmp,tmp,ROMW_A0_BT
	  or	  tmp,tmp,wbase
	  or	  tgt,tgt,tmp
 .endm


    ;handy registers
	greg	am29_cycle_1,		;EEPROM command preamble
	greg	am29_cycle_2
	greg	wh5555,			;common EEPROM addresses
	greg	wh2aaa

	greg	p_fsi,			;FSI registers
	greg	p_fsifcr,
	greg	p_mac,			;MAC registers
	greg	p_elm,			;ELM registers

	greg	p_gsor,			;pointer to GSOR
	greg	p_blur,			;pointer to BLUR
	greg	p_creg,			;pointer to CREG
	greg	cregs,			;CREG shadow
	greg	green,			;bits in the CREG
	greg	red,
	greg	leds,

	greg	wbase,			;base of EPROM for writing

	greg	pstep,			;2=erasing, 3=programming

	greg	src,			;source to program
	greg	psize,			;size of the EPROM
	greg	pval,
	greg	rptr,
	greg	wptr,

	greg	pret,

	greg	tcnt,			;for timing
	greg	tret,

	.sect	fprog,text, absolute XPI_PROG
	.use	fprog
	.global	_main

_main:
	mtsrim	CPS, PS_INTSOFF
	constx	t0, CFG_PMB_16K | CFG_ILU
	mtsr	CFG, t0
	xor	gr1,gr64,gr64

    ;initialize the common registers
	constx	wbase, EPROM_WRITE

	constx	am29_cycle_1,AM29_CYCLE_1
	constx	am29_cycle_2,AM29_CYCLE_2
	constx	wh5555,	WH5555
	constx	wh2aaa,	WH2AAA

	constx	p_fsi,	FSIBASE
	constx	p_fsifcr,FSI_GSR
	constx	p_mac,	MACBASE
	constx	p_elm,	ELMBASE
	constx	p_gsor,	GSOR
	constx	p_blur,	BLUR
	constx	p_creg,	CREG
	constx	green,	CREG_GREEN
	constx	red,	CREG_RED
	or	leds,	green,red


	or	cregs,leds,0
	store	0,WORD,cregs,p_creg	;turn off both LEDs

	const	t0,0
	store	0,WORD,t0,p_blur	;just to be safe

	load	0,WORD,t0,p_gsor	;clear old host interrupt

    ;Try to ensure that the FSI is not doing anything.  This is in
    ;case the firmware has been restarted without a hardware reset.
	STO_IR	RESET,0,0,t0
	STOFSI	0,IMR,t0,t1
	const	t0, 0
	STOMAC	t0, CTRL_A, t1
	STOELM0	t0, MASK, t1
	STOELM1	t0, MASK, t1

    ;set both ELMs to QLS, because we are not repeating line states
    ;and symbols correctly.
	constx	t1, ELM_CA_DEFAULT | ELM_CA_SC_BYPASS | ELM_CA_REM_LOOP
	STOELM0	t1, CTRL_A,t0
	STOELM1	t1, CTRL_A,t0
	constx	t1, ELM_CB_PCM_OFF
	STOELM0	t1, CTRL_B,t0
	STOELM1	t1, CTRL_B,t0

	AM29	RESET,t0		;reset the EEPROM

	andn	cregs,cregs,red		;turn on the red LED
	store	0,WORD,cregs,p_creg

	call	pret,erase		;erase the EEPROM
	 const	pstep,2

	call	pret,prog		;then write it
	 const	pstep,3


;success
	inv				;we have changed memory
	AM29	RESET,t0		;just in case

good:	andn	cregs,cregs,green
	store	0,WORD,cregs,p_creg	;flash the green LED
	DELAY	100*1000

	or	cregs,cregs,leds	;turn off both LEDs
	store	0,WORD,cregs,p_creg
	DELAY	100*1000

	mfsr	t0,CPS
	tbit	t0,t0,PS_IP,		;wait for the host to poke us
	jmpf	t0,good

	 const	t0,0
	jmpi	t0			;then run the firmware
	 nop



;erase it
;   enter with pret=return
erase:
	AM29	ERASE,t0,		;tell the chip to start
	AM29	ERASE_CHIP,t0

    ;Wait until the EEPROM says it is finished.
erase5:
	DELAY	100
	load	0,BYTE,t0,0
	cpeq	t1,t0,0xff
	jmpti	t1,pret

	 tbit	t2,t0,DQ5
	jmpf	t2,erase5
	 nop

	load	0,BYTE,t3,0
	cpeq	t4,t3,0xff
	jmpti	t4,pret
	 nop
	jmp	fail
	 nop


;program the chip
;   enter with pret=return
prog:
	constx	rptr,EPROM
	constx	psize,EPROM_SIZE-2
	constx	src,XPI_DOWN		;write from here

prog1:
	load	0,BYTE,pval,src		;get next byte to program

	cpeq	t0,pval,0xff		;skip it if already right
	jmpt	t0,prog8
	 nop

	AM29	PGM,t0
	MAKWPTR wptr,rptr,t0
	sll	t0,pval,24		;left justify new value
	store	0,WORD,t0,wptr		;write byte to EEPROM

	DELAY	9
prog5:
	DELAY	1
	load	0,BYTE,t0,rptr
	cpeq	t1,t0,pval
	jmpt	t1,prog8

	 tbit	t2,t0,DQ5
	jmpf	t2,prog5
	 nop

	load	0,BYTE,t3,rptr
	cpeq	t4,t3,pval
	jmpt	t4,prog8
	 nop
	jmp	fail
	 nop

prog8:
	add	src,src,1		;advance the address to the next byte
	jmpfdec	psize,prog1		;do next byte
	 add	rptr,rptr,1

	jmpi	pret			;finished
	 nop




;failed.  Reset the chip and flash the red LED 1, 2, or 3 times
;   pstep=2 or 3 to indicate whether the erasing or programming failed.
fail:
	AM29	RESET,t0

fail1:	sub	t0,pstep,2

fail2:	andn	cregs,cregs,red
	or	cregs,cregs,green
	store	0,WORD,cregs,p_creg
	DELAY	50*1000

	or	cregs,cregs,red
	store	0,WORD,cregs,p_creg
	DELAY	75*1000

	jmpfdec	t0,fail2
	 nop

	or	cregs,cregs,red
	store	0,WORD,cregs,p_creg
	DELAY	500*1000

	jmp	fail1
	 nop


;delay for a few clock ticks
delay:	jmpfdec	tcnt,.
	 mtsrim	MMU,0xff
	jmpi	tret
	 nop


    ;coff2firm assumes the last word is the version number
	.word	VERS

    ;there better be room for the checksum
	CK	. < XPI_PROG + XPI_PROG_SIZE
	.end
