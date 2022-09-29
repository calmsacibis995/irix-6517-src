;program FLASH EEROM on GIO bus FDDI card

; Copyright 1992 Silicon Graphics, Inc.  All rights reserved.
; "$Revision: 1.8 $"

	.file	"xpi_flash.s"

	;XXX should be in giolc.h.
	;XXX needed in std.h, but giolc.h needs std.h
	.equ	AM29030,1

	.include "std.h"
	.include "gio.h"

	.include "if_xpi_s.h"

	.equ	VERS,((((VERS_Y-92)*13+VERS_M)*32+VERS_D)*24+VERS_H)*60+VERS_MM

    ;chip ID
	.equ	CAT_MFG,    0x31	;Catalyst vendor code
	.equ	CAT_28F010, 0xb4	;28F010


    ;commands for the EEROM
	.equ	ERASE_CMD,	0x20000000
	.equ	ERASE_VRFY_CMD,	0xa0000000
	.equ	PRG_VERIFY_CMD,	0xc0000000
	.equ	WRITE_CMD,	0x40000000
	.equ	SPEED_WRITE_CMD,0xe0000000
	.equ	READ_CMD,	0x00000000
	.equ	READ_SIG,	0x90000000
	.equ	RESET_CMD,	0xff000000

	.equ	ERASE_PLSLIMIT,	3000	;do not zap it more than this
	.equ	WRITE_PLSLIMIT,	25	;max write attempts/byte

	DEFBT	ROMW_A0,18		;ROM a0 for writes

	.equ	E_DELAY,    10*1000	;delay afer erase
	.equ	EV_DELAY,   6		;delay after erase-verify command
	.equ	PV_DELAY,   EV_DELAY	;delay after program-verify command
	.equ	WR_DELAY,   10		;delay after write command

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


;reset the EEPROM
;	Intel recommends that the very first thing done is to make sure
;	the flash is not in some funny state by issueing two (2) 0xff
;	bytes to the FLASH device.  0xff is the "reset" command to
;	internal flash sequencing logic.  The first 0xff will satisfy
;	the sequencer's data request if it is in program mode without
;	doing anything.  0xff is the unprogrammed state, hence an 0xff
;	will be seen as a program "nop" or as the first of two reset
;	commands.  The the second 0xff will then be interpreted as either a
;	first or second	reset command.
 .macro	EERESET, tmp
	  consth  tmp,RESET_CMD
	  store	  0,WORD,tmp,wbase
	  DELAY	  WR_DELAY
	  store	  0,WORD,tmp,wbase
 .endm


;make a flash write pointer from a read pointer at the same location
 .macro	MAKWPTR, tgt,rp,tmp
	  and	  tmp,rp,3
	  andn	  tgt,rp,3
	  sll	  tmp,tmp,ROMW_A0_BT
	  or	  tmp,tmp,wbase
	  or	  tgt,tgt,tmp
 .endm


    ;some constant value registers
	greg	ecmd,			;EEPROM commands
	greg	evcmd,
	greg	rcmd,
	greg	wcmd,
	greg	s_wcmd,
	greg	pvcmd,

	greg	dofast,			;true if word-mode available

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

	greg	pstep,			;1=zeroing, 2=erasing, 3=programming

	greg	src,			;source to program
	greg	psize,			;size of the EPROM
	greg	pval,
	greg	zapcnt,			;limit chip zapping
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

	constx	wbase, EPROM_WRITE

	constx	rcmd,	READ_CMD
	constx	wcmd,	WRITE_CMD
	constx	s_wcmd,	SPEED_WRITE_CMD
	constx	pvcmd,	PRG_VERIFY_CMD
	constx	ecmd,	ERASE_CMD
	constx	evcmd,	ERASE_VRFY_CMD

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

    ;reset the EEPROM
	EERESET	t0

    ;see if can use the Catalyst method
	constx	dofast,READ_SIG
	store	0,WORD,dofast,wbase
	load	0,HWORD,dofast,0
	const	t0,CAT_MFG*256 + CAT_28F010
	cpeq	dofast,dofast,t0

	andn	cregs,cregs,red		;turn on the red LED
	store	0,WORD,cregs,p_creg

	call	pret,clr_sub		;make everything zeros
	 const	pstep,1

	call	pret,erase_sub		;then ones
	 const	pstep,2

	call	pret,prog_sub		;then desired value
	 const	pstep,3


;success
	inv				;we have changed memory

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



;clear the chip
;   enter with pret=return
clr_sub:
	constx	rptr,EPROM

	jmpf	dofast,clr5

    ;be fast if possible
	 constx	psize,EPROM_SIZE/4-2
clr2:
	load	0,WORD,t0,rptr

	constx	zapcnt,0x80000000+WRITE_PLSLIMIT
	MAKWPTR wptr,rptr,t1

	cpeq	t0,t0,0
	jmpt	t0,clr4
	 const	pval,0
clr3:
	jmpfdec	zapcnt, fail		;give up on a bad word
	 nop

	store	0,WORD,s_wcmd,wbase	;WRITE_CMD
	store	0,WORD,pval,wptr	;write byte to EEPROM
	DELAY	WR_DELAY

	store	0,WORD,pvcmd,wptr	;send program verify command
	DELAY	PV_DELAY
	load	0,WORD,t0,rptr		;read programmed byte

	cpeq	t0,t0,0			;flash == 0 ?
	jmpf	t0, clr3		;no, try to write it again
	 nop

clr4:
	jmpfdec	psize,clr6		;do next byte
	 add	rptr,rptr,4

	jmpi	pret			;finished
	 store	0,WORD,rcmd,wbase	;stop special stuff


    ;do it the Intel way
clr5:
	constx	psize,EPROM_SIZE-2

clr6:
	load	0,BYTE,t0,rptr

	constx	zapcnt,0x80000000+WRITE_PLSLIMIT
	MAKWPTR wptr,rptr,t1

	cpeq	t0,t0,0
	jmpt	t0,clr8
	 const	pval,0
clr7:
	jmpfdec	zapcnt, fail		;give up on a bad byte
	 nop

	store	0,WORD,wcmd,wbase	;WRITE_CMD
	store	0,WORD,pval,wptr	;write byte to EEPROM
	DELAY	WR_DELAY

	store	0,WORD,pvcmd,wptr	;send program verify command
	DELAY	PV_DELAY
	load	0,BYTE,t0,rptr		;read programmed byte

	cpeq	t0,t0,0			;flash == 0 ?
	jmpf	t0, clr7		;no, try to write it again
	 nop

clr8:
	jmpfdec	psize,clr6		;do next byte
	 add	rptr,rptr,1

	jmpi	pret			;finished
	 store	0,WORD,rcmd,wbase	;stop special stuff



;erase it
;   enter with pret=return
erase_sub:
	constx	rptr,EPROM
	constx	psize,EPROM_SIZE-2

	constx	zapcnt,0x80000000+ERASE_PLSLIMIT
erase1:
	jmpfdec	zapcnt,fail		;give up on a bad chip
	 nop

	store	0,WORD,ecmd,wbase	;send an erase command
	store	0,WORD,ecmd,wbase	;sequence to the flash
	DELAY	E_DELAY

erase5:
	MAKWPTR wptr,rptr,t0
	store	0,WORD,evcmd,wptr	;send it an erase-verify command
	DELAY	EV_DELAY
	load	0,BYTE,t0,rptr		;read a byte from the chip

	cpeq	t0,t0,0xff		;is it erased?
	jmpf	t0,erase1		;no, zap the chip again
	 nop

	jmpfdec	psize,erase5		;continue until finished
	 add	rptr,rptr,1		;increment read pointer

	jmpi	pret
	 store	0,WORD,rcmd,wbase	;stop special stuff


;program the chip
;   enter with pret=return
prog_sub:
	constx	rptr,EPROM
	constx	psize,EPROM_SIZE-2
	constx	src,XPI_DOWN		;write from here

prog1:
	constx	zapcnt,0x80000000+WRITE_PLSLIMIT
	MAKWPTR wptr,rptr,t0

	jmpf	dofast,prog5

	 and	t0,src,3		;at a word boundary?
	cpeq	t0,t0,0
	jmpf	t0,prog5
	 nop
	load	0,WORD,pval,src
	cpeq	t0,pval,0		;and is it 0
	jmpf	t0,prog5
	 nop

    ;program a word using the Catalyst "speed write" feature
prog2:
	jmpfdec	zapcnt, fail		;give up on a bad word
	 nop

	store	0,WORD,s_wcmd,wbase
	store	0,WORD,pval,wptr	;program a word of zero
	DELAY	WR_DELAY

	store	0,WORD,pvcmd,wptr
	DELAY	PV_DELAY
	load	0,WORD,t0,rptr		;read all 4 bytes of the word

	cpeq	t0,t0,pval		;flash == pattern ?
	jmpf	t0,prog2		;no, try to write it again
	 nop

	add	src,src,3		;look at the next word
	add	rptr,rptr,3
	jmp	prog8
	 sub	psize,psize,3


    ;program a byte the Intel way
prog5:
	load	0,BYTE,pval,src		;get next byte to program

	cpeq	t0,pval,0xff
	jmpt	t0,prog8
	 nop

prog7:
	jmpfdec	zapcnt, fail		;give up on a bad byte

	 sll	t0,pval,24		;left justify new value
	store	0,WORD,wcmd,wbase	;WRITE_CMD
	store	0,WORD,t0,wptr		;write byte to EEPROM
	DELAY	WR_DELAY

	store	0,WORD,pvcmd,wptr
	DELAY	PV_DELAY
	load	0,BYTE,t0,rptr		;read programmed byte

	cpeq	t0,t0,pval		;flash == pattern ?
	jmpf	t0, prog7		;no, try to write it again
	 nop

prog8:
	add	src,src,1		;advance the address to the next byte
	jmpfdec	psize,prog1		;do next byte
	 add	rptr,rptr,1

	jmpi	pret			;finished
	 store	0,WORD,rcmd,wbase	;stop special stuff




;failed.  Reset the chip and flash the red LED 1, 2, or 3 times
;   pstep=1,2, or 3 to indicate whether the zeroing, erasing, or programming
;	failed
fail:
	EERESET	t0
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
