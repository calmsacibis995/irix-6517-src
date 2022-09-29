; (c) Copyright 1986 Central Data Corporation
; All Rights Reserved
;
; This program is an unpublished work fully protected by the
; United States Copyright laws and is considered a trade secret
; belonging to Central Data Corporation.

; RESTRICTED RIGHTS LEGEND

; Use, duplication, or disclosure by the Government is subject to
; restrictions as set forth in subdivision (b) (3) (ii) of the
; Rights in Technical Data and Computer Software clause at 52.227-7013.

page
; 3608beg.drv - Beginning of Code
;
include 3608equ.h

;**********************************************************************
;
stk	segment	word	stack	'stack'
stk	ends
cseg	segment	word	public	'code'
	assume	cs:cseg

extrn	main:near

	public	istart
istart	proc	near
;
;This is the beginning of the code:
;
comment{ This routine is jumped to at the completion of the power on
	tests. It is responsible for setting up the interrupt vectors
	in scratchpad ram, and the internal registers of the 8088.
	It also initializes offsets 1fe0h-1fffh of each dual-port ram
	buffer. 
{
;
	cli
	xor	ax,ax		;initialize registers:
	mov	ds,ax			;ds=0 (scratchpad ram)
	mov	es,ax
;
	mov	ss,ax			;ss=0000 (stack 007ff)
	mov	sp,07ffh
;
	mov	cx,800h
	xor	di,di
	cld
   rep	stosb			;clear scratchpad ram address 0 - 7ffh (2kx8)

;more than minimal interrupt vectors
	mov	word ptr ds:0*4,offset spur 
	mov	word ptr ds:0*4+2,0fc00h
	mov	word ptr ds:1*4,offset spur
	mov	word ptr ds:1*4+2,0fc00h
	mov	word ptr ds:3*4,offset spur
	mov	word ptr ds:3*4+2,0fc00h
	mov	word ptr ds:4*4,offset spur
	mov	word ptr ds:4*4+2,0fc00h

;clear bytes 1fe0-1fff in each dual-port buffer.
	mov	dx,dpa0			;set es=base addr dp0
	mov	es,dx
	xor	si,si
	xor 	ax,ax
clrdpr:	lea	di,[si+pad1]
	cld
	mov	cx,dplen-pad1
    rep	stosb

	lea	cx,ds:[si+ib]
	mov	es:[si+ifp],cx		;initialize input fill pointer

	add	si,dplen		;advance to next port
	jnc	clrdpr			;(this assumes 8*dplen=64K)

	mov	word ptr ds:[idlyval],4000h	;infrequent interrupts at first

;write software revision number version in port0.
	mov 	word ptr es:[rev], high(vers) + (low(vers) shl 8)

	out	10h,al		;turn on LED 1 and LED 2 off
	out	19h,al

	out	14h,al		;set for polled mode

;Initialize all eight ports.
	initscc

	mov	bx,po0		;base addr in sram for port 0 write registers
	mov	dx,scc0		;addr for scc0
cic:	in	al,dx	;next, check for input chars
	test	al,01h		;read reg 0 bit 0 set?
	jz	wrust0	;no- continue
	inc	dx		;else read char
	in	al,dx
	dec	dx
	jmp	cic		;and check for others

;begin programming SCCs
;
wrust0:	mov	al,04h
	out	dx,al		;point to reg4 (wr4)
	nop				;insert nops to assure 16 clocks
	nop
	nop
  	mov	al,4ch		;4ch--x16 clk, 2 stop bits/char, parity disabled
	out	dx,al		;write data to reg 4
	mov	ds:[bx+wr4],al 	;save data
;
	nop
	mov	al,10
	out	dx,al		;point to wr10
	nop
	nop
	nop
	mov	al,00h		;00h nrz coding
	out	dx,al		;load data into reg
	mov	ds:[bx+wr10],al	;save data
;
	nop
	mov	al,11
	out	dx,al		;point to wr11
	nop
	nop
	nop
	mov	al,56h		;56h=Rx and Tx clks and TRxC out are BR
	out	dx,al		;load data into reg
;
	nop
	nop
	nop
	mov	al,12
	out	dx,al		;point to wr12
	nop
	nop
	nop
	mov	al,06h		;baud rate = 9600, time cons =06h
	out	dx,al		;load data into reg
	mov	ds:[bx+wr12],al	;save data
;
	nop
	mov	al,0dh
	out	dx,al		;point to wr13
	nop
	nop
	nop
	mov	al,00h		;upper byte of time cons for baud rate
	out	dx,al		;load data into reg
	mov	ds:[bx+wr13],al	;save data
;
	nop
	mov	al,14
	out	dx,al		;point to wr14
	mov	al,03h		;03h=BR enabled,clk src for BRG is clk in
	nop
	nop
	nop
	out	dx,al		;load data into reg
	mov	ds:[bx+wr14],al	;save data
;
	nop
	mov	al,15
	out	dx,al		;point to wr15
	nop
	nop
	nop
	mov	al,00h		;status int disabled
	out	dx,al		;load data into reg
;
	nop
	nop
	nop
	mov	al,9
	out	dx,al		;point to wr9
	nop
	nop
	nop
	mov	al,00h		;ints disabled
	out	dx,al		;load data into reg
;
	nop
	nop
	nop
	mov	al,5
	out	dx,al		;point to wr5
	mov	al,068h		;8 bits\Tx char,Tx en
	nop
	nop
	out	dx,al		;load data into reg
	mov	ds:[bx+wr5],al	;save data
;
	nop
	mov	al,03h
	out	dx,al		;point to wr3
	nop
	nop
	nop
	mov	al,0c0h		;8 bits\Rx char, Rx off
	out	dx,al		;load data into reg
	mov	ds:[bx+wr3],al	;save data
;
	nop
	mov	al,1
	out	dx,al		;point to wr1
	nop
	nop
	nop
	mov	al,00h		;no interrupts
	out	dx,al

	add	bx,padlen	;get base next port
	cmp	dx,scc7		;done?
	je	wrust1
	add	dx,08h		;no--get next addr
	jmp	cic


wrust1:	out	30h,al		;tell the host we are alive
	jmp	main		;all set - goto main program

istart	endp


;	**** Spur (spurious interrupt handler) ****
;
spur	proc	near
	hlt
spur	endp
;
cseg	ends
	end
;
