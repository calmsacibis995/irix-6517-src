;program FLASH EEROM on E-Plex 8-port Ethernet card

; Copyright 1994 Silicon Graphics, Inc.  All rights reserved.
; "$Revision: 1.5 $"

	.file	"ep_flash.s"

	.equ	AM29030,1
	.include "std.h"
	.include "ephwd.h"
	.include "if_ep_s.h"

	.equ	VERS,(((VERS_Y-92)*13+VERS_M)*32+VERS_D)*24+VERS_H

	DEFBT	ROMW_A0,18		;ROM a0 for writes

    ;commands for the EEROM
	.equ	AM29_CYCLE_1,	0xaa
	.equ	AM29_CYCLE_2,	0x55
	.equ	AM29_RESET,	0xf0
	.equ	AM29_PGM,	0xa0
	.equ	AM29_SIG,	0x90
	.equ	AM29_ERASE,	0x80
	.equ	AM29_ERASE_CHIP,0x10

	.equ	A2AAA,		0x2aaa	;magic addresses
	.equ	A5555,		0x5555

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
	  store	  0,BYTE,am29_cycle_1,a5555
	  store	  0,BYTE,am29_cycle_2,a2aaa
	  constx  tmp,AM29_@cmd@
	  store	  0,BYTE,tmp,a5555
 .endm

    ;handy registers
	greg	am29_cycle_1,		;EEPROM command preamble
	greg	am29_cycle_2
	greg	a5555,			;common EEPROM addresses
	greg	a2aaa

	greg	p_leds
	greg	leds

	greg	pstep,			;2=erasing, 3=programming

	greg	src,			;source to program
	greg	psize,			;size of the EPROM
	greg	pval,
	greg	rptr,

	greg	pret,

	greg	tcnt,			;for timing
	greg	tret,

	.sect	fprog,text, absolute EP_PROG
	.use	fprog
	.global	_main

_main:
	mtsrim	CPS, PS_INTSOFF
	constx	t0, CFG_PMB_16K | CFG_ILU
	mtsr	CFG, t0
	xor	gr1,gr64,gr64

    ;initialize the common registers
	constx	am29_cycle_1,AM29_CYCLE_1
	constx	am29_cycle_2,AM29_CYCLE_2
	constx	a5555,	A5555
	constx	a2aaa,	A2AAA

	constx	p_leds,LEDS
	const	leds,0			;turn off all LEDs
	store	0,WORD,leds,p_leds

	constx	t0,INTC
	constx	t1,INTC_H2BINT
	store	0,WORD,t1,t0		;clear old host interrupt
	
	constn	t0,-1
	constx	t1,SONIC_RESET		;reset the SONICs
	store	0,WORD,t0,t1

	const	t0,0
	constx	t1,GSOR
	store	0,WORD,t0,t1		;turn off all interrupts

	AM29	RESET,t0		;reset the EEPROM

	const	leds,LED_YELLOW		;turn on the yellow LED
	store	0,WORD,leds,p_leds

	call	pret,erase		;erase the EEPROM
	 const	pstep,2

	call	pret,prog		;then write it
	 const	pstep,3


;success
	inv				;we have changed memory
	AM29	RESET,t0		;just in case

good:	add	leds,leds,LED_GREEN0	;flash a green LED
	and	leds,leds,LED_GREEN0 | LED_GREEN1 | LED_GREEN2
	store	0,WORD,leds,p_leds
	DELAY	100*1000

	constx	t1,GSOR
	load	0,WORD,t0,t1
	tbit	t0,t0,INTC_H2BINT,	;wait for the host to poke us
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
	constx	src,EP_DOWN		;write from here

prog1:
	load	0,BYTE,pval,src		;get next byte to program

	cpeq	t0,pval,0xff		;skip it if already right
	jmpt	t0,prog8
	 nop

	AM29	PGM,t0
	store	0,BYTE,pval,rptr	;write byte to EEPROM

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




;failed.  Reset the chip and flash the yellow LED 1, 2, or 3 times
;   pstep=2 or 3 to indicate whether the erasing or programming failed.
fail:
	AM29	RESET,t0

fail1:	sub	t0,pstep,2

fail2:	const	leds,LED_YELLOW
	store	0,WORD,leds,p_leds
	DELAY	75*1000

	const	leds,0
	store	0,WORD,leds,p_leds
	DELAY	50*1000

	jmpfdec	t0,fail2
	 nop

	store	0,WORD,leds,p_leds
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
	CK	. < EP_PROG + EP_PROG_SIZE
	.end
