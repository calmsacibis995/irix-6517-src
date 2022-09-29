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
; 3608host.drv - Host Commands
;
include 3608equ.h

cseg	segment	word	public	'code'
	assume	cs:cseg

extrn	istart:near
extrn	main:near


;This table holds the addresses of the different command subroutines.
;
calcmd	dw	ssr	;set scc mode registers
	dw	sbr	;set baud rate
    	dw	oflush	;flush output
	dw	illcmd
	dw	sb0	;send block 0
	dw	sb1	;send block 1
	dw	illcmd
	dw	sbrk	;send break
	dw	sindt	;set input delay time
	dw	illcmd
	dw	illcmd
	dw	illcmd
	dw	illcmd
	dw	sinti	;set input interrupt
	dw	illcmd
	dw	illcmd
	dw	illcmd
	dw	illcmd
	dw	istart	;re-enter the initialization routine
	dw	illcmd
	dw	illcmd
numcmd	equ	($-calcmd)/2

;	**** process host command ****
	public	prhcmd
prhcmd	proc	near
;
; At entry, it has been determined that there is a new command
;	from the host. The i/o byte which indicates
;	which ports have commands pending (bit0=prt 0,bit1=prt 1 etc)
;	is read. Each bit of the i/o byte is checked, and if equal to 1 the
;	command byte for that port is checked and if legal is
;	executed. If the command was illegal, the command status
;	byte is set to 01, otherwise it is cleared. When all the
;	requested commands have been executed or initiated, the
;	command complete flip-flop is set and this routine is exited.

; On entry ES,SS=base of dual port SRAM,  DS=base of scratch pad SRAM,
; On exit only ES,SS, and DS are preserved.

	in	al,21h
	mov	ch,al			;keep command mask in CH
	mov	dx,scc0-8		;SCC address
	mov	di,po0-padlen		;DI=port variables
	mov     bp,-dplen		;for dual-port segment

;Adjust registers to next port. If done, set the command complete flip-flop
;	and return.
cknxt:	add	bp,dplen		;ready for BP = next port
	add	dx,08h			;set DX to next scc addr
	add	di,padlen		;set DI to next port variables
	shr	ch,1			;does this port need service?
	jc	docmd			;yes, fix it.
	jnz	cknxt

	out	30h,al			;set command complete flip-flop

	xchg    ch,es:[intr]
	or	ch,ch
	jz	j_main
	or	ds:[intbits],ch
	mov     ax,ds:[clk]
	mov     ds:[idlyclk],ax		;note requested host interrupt
j_main: jmp     main


;This port has a command. Load the command into BL, and see if it
;is a legal command number. 
docmd:	xor	bx,bx
	mov	bl,ss:[cmdcd+bp]	;load command byte
	cmp	bl,numcmd		;legal command?
	jae	illcmd

;Call the subroutine that handles the desired command.
;Each command has CL,SI,BX=scratch,  BP=dual port area,  DX=SCC address,
;    DI=scratch pad, SS,ES=dual port segment,  CH=request bit map
	mov	ax,ss:[cmdd+bp]		;load cmd data
	shl	bx,1
	jmp	calcmd[bx]

goodcmd:mov	byte ptr ss:[cmds+bp],00h	;set cmd status=00 normal cmpl
	jmp	cknxt

;obsolete or illegal command
illcmd:	mov	byte ptr ss:[cmds+bp],01h	;set cmd status=01 illegal cmd
	jmp	cknxt			;check next port

prhcmd	endp


;********************************************************************

;	**** Ssr (set scc registers) ****
;
ssr	proc	near
;check for illegal register
	cmp	ah,03h		;register 0,1,or 2?
	jb	illcmd
	je	reg3
;
	cmp	ah,04h		;reister 4?
	je	reg4
;
	cmp	ah,05h		;reister 5?
	je	reg5
;
	cmp	ah,0eh		;register 6,7,8,9,10,11,12,13?
	jb	illcmd			;yes-goto error
	je	reg14
	jmp	illcmd

;test the data for invalid bits
;
reg3:	test	al,1eh	;bits 1-4 = 0?
	jnz	illcmd			;no - error
	lea	bx,[di+wr3]
	jmp	wreg
;
reg4:	test	al,30h	;bits 4,5 = 0?
	jnz	illcmd			;no--error
	test	al,0ch		;bits 2 and 3 both = 0?
	jz	illcmd			;yes-- error
	lea	bx,[di+wr4]
	jmp	wreg
;
reg5:	test	al,15h	;bits 0,2,4 = 0?
	jnz	illcmd			;no - error

	lea	bx,[di+wr5]
	jmp	wreg

reg14:	test	al,0e0h	;bits 5-7 = 0?
	jnz	illcmd
	test	al,01h	;bit 0 = 1?
	jz	illcmd
	test	al,02h	;bit 1 = 1?
	jz	illcmd
	lea	bx,[di+wr14]

;Write SCC register
;  here AL=data, AH=register number
wreg:	xchg	al,ah
	out	dx,al		;point to SCC register
	xchg	al,ah
	mov	ds:[bx],al	;save new value in SRAM
	out	dx,al		;write data to SCC register
	jmp	goodcmd
ssr	endp


;	****Sbr (set baud rate)******
;
sbr	proc	near
;
	mov	ds:[di+wr12],ah	;save new time constant
	mov	ds:[di+wr13],al

	mov	al, 14		;turn off bit clock in wr14
	out	dx,al

	mov	al,ds:[di+wr14]
	and	al,0feh
	out	dx,al

	mov	al,12		;write wr12 part of time constant
	nop
	nop
	out	dx,al

	mov	al,ds:[di+wr12]
	out	dx,al

	mov	al,13		;write wr13 part of time constant
	nop
	nop
	out	dx,al

	mov	al,ds:[di+wr13]	
	out	dx,al

	mov	al,14
	nop
	nop
	out	dx,al

	mov	al,ds:[di+wr14]
	out	dx,al		;reenable rate generator

;Preset the variable area in scratchpad ram for port x
	xor	ax,ax
	lea	bx,[di+padvlen-2]
clrvar:	mov	ds:[bx],ax
	sub	bx,2
	cmp	bx,di
	jae	clrvar

	mov	ss:[opb+bp],al		;clear output busy flag
	jmp	goodcmd
sbr	endp


;	**** Sidt (set input delay time) ****
; AX=big-endian value in milliseconds, never > 142 to prevent input overflow
sindt	proc	near
	mov	al,1000/clkres
	mul	ah
	mov	ds:[idlyval],ax
	jmp	goodcmd
sindt	endp


;	**** Flush Output ****
oflush	proc	near
	xor	ax,ax
	mov	ds:[di+wptr],ax
	mov	ds:[di+wptr2],ax
	mov     ds:[di+txon],al
	mov	ss:[opb+bp],al
	jmp	goodcmd
oflush	endp


;	**** Sb0 (send block 0) ****
sb	proc	near
sb0:	lea	si,ds:[bp+ob0]
	jmp	ixmit

;	**** Sb1 (send block 1) ****
sb1:	lea	si,ds:[bp+ob1]

ixmit:	xchg	al,ah
	dec	ax
	add	ax,si

	cmp	word ptr ds:[di+wptr],0	;already busy?
	jne	ixmitd			;yes, delay this request

	mov	ds:[di+wptr],si		;no, save starting pointer
	mov	ds:[di+wend],ax		;and end pointer
	and	byte ptr ds:[di+prvst],not RR0_TXE
	mov	byte ptr ds:[di+txon],RR0_TXE
	mov	byte ptr ss:[opb+bp],0ffh	;set output busy flag=busy
	jmp	goodcmd

ixmitd:	mov	ds:[di+wptr2],si	;save starting pointer
	mov	ds:[di+wend2],ax	;and end pointer
	;the host has already set output busy flag=very busy
	jmp	goodcmd
sb	endp


;	**** Sbrk (send break) ****
;
sbrk	proc	near
	lea	ax,ds:[bp+ob0]
	mov	ds:[di+wend],ax		;end before current to signal BREAK
	inc	ax
	mov	ds:[di+wptr],ax

	xor	ax,ax
	mov	ds:[di+wptr2+1],ax	;no following buffer yet

	and	byte ptr ds:[di+prvst],not RR0_TXE	;watch TX ready
	mov	ds:[di+txon],al		;turn off normal output
	dec	ax
	mov	ss:[opb+bp],ax		;set o/p busy flag=ff(busy)
	jmp	goodcmd
sbrk	endp

;	**** Sinti (set input interrupt) ****
;
sinti	proc	near
	mov	ds:[intvec],ah
	and	ax,000fh
	mov	ds:[intlvl],ax		;save the level
	out	1eh,al			;select ROAK mode

	and	al,0007h
	mov	dx,ax
	out	dx,al
	jmp	goodcmd
sinti	endp

cseg	ends
	end
