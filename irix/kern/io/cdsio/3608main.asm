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
; These assumptions are handy to make the board fast.  Most of them are
; a matter of making things faster by doing less.
; 
; Assume the host can keep up, and do not count input overflows.
; Since the host is keeping up, it is running RTS and XON/XOFF.
; 
; The speed of the code is almost entirely determined by fetching it and 
; so by its length.  There are slow 8088 opcodes.  This means the normal
; attention to variable timings of conditional jumps and so on would be
; misplaced.
; 
; Assume the host makes no errors.  Assume it does not tell us to do 
; anything silly.
; 
; Since there is no FIFO in the path output of the SCC, and we must output
; at 38.4, the loop must take less than 260 usec or 2080 clocks.
; 
; When input is active, output is probably also needed.
; 
; Do not waste BP and SS on a one-deep function call stack when they
; can reduce the number of segment overrides.
; 
; Input can be checked as much a 3 times less often than output because of
; the 3-byte input FIFO.
; 
; The host can do strange things like byte-swapping and subtracting 2 to
; decode the input-filling pointer.


include 3608equ.h

;do input for a port
doin	macro	pn
	local	morin, stok

	mov	di,ss:[ifp+do&pn&+bp]	;fetch input buffer pointer
	mov	ah,ds:[brkbit+po&pn&]	;fetch BREAK bit from previous status
morin:	mov	al,1			;point at WR1
	out	scc&pn&,al

	and	di,not inlen

	in	al,scc&pn&		;fetch RR1 status bits
	and	al,RR1_PE+RR1_OE+RR1_FE
	or	ah,al			;combine BREAK and status bits
	in	al,scc&pn&+1		;fetch data byte
	stosw				;give status and data to the host

	jz	stok		
	mov	al,WR0_ER		;reset status bits
	out	scc&pn&,al
	mov	byte ptr ds:[brkbit+po&pn&],0

stok:	in	al,scc&pn&		;more input available?
	test	al,RR0_RX
	jnz	morin			;yes, handle it

	mov	ss:[ifp+do&pn&+bp],di	;save new input buffer pointer

	or	ch,intbit_in		;trigger the interrupt timer
	endm


;send a byte to a port
; AL	- RR0
doout	macro	pn, odone, ocont
	local	exit

	test	al,ds:[txon+po&pn&] 	;TX needed and possible?
    ifidn <n/a>,<ocont>
	jz	exit			;no, try the next port
    else
	jz	ocont			;no, try the next port
    endif

	mov	di,ds:[wptr+po&pn&]
	mov	al,ss:[di+bp]
	out	scc&pn&+1,al		;output the next data byte

    ifidn <n/a>,<ocont>
	cmp	di,ds:[wend+po&pn&]
	je	odone			;wrap up if that was the last byte
	inc	di
	mov	ds:[wptr+po&pn&],di 	;save new pointer
exit:
    else
	inc	di
	mov	ds:[wptr+po&pn&],di 	;save new pointer
	cmp	di,ds:[wend+po&pn&]
	jbe	ocont
	jmp	odone			;wrap up if that was the last byte
    endif
	endm


;fast output
fasto	macro	pn, odone,ocont
	in	al,scc&pn&
	doout	pn, odone,ocont
	endm

;fast input and output
fastio	macro	pn, odone, ocont
	local	odat

	in	al,scc&pn&
	test	al,RR0_RX
	jz	odat
	doin	pn

odat:	doout	pn, odone,ocont
	endm


;check the status of a port
; pn	 - port #
; newst - go there if status has changed
ckio	macro	pn, oldst, newst
	in	al,scc&pn&
	cmp	al,ds:[prvst+po&pn&] 	;has the status changed?
	je	oldst
	jmp	newst			;yes, work on it
	endm


;completely service a port
;al=RR0
; pn	- port #
; exit	- go there after doing work
svport	macro	pn, exit
	local	odat,j_exit,j_oend,bout,bstrt,nobrk,noodat,dcdint,sndst

	add	bx,(100+clkres/2)/clkres

	test	al,1			;ready for input?
	jz	odat			;no, try output
	doin	pn			;yes, get it all

;see if the SCC is ready for some output
;al=RR0
odat:	test	al,RR0_TXE		;ready for more output?
	jz	noodat

	mov	di,ds:[wptr+po&pn&] 	;yes, but is a write needed?
	or	di,di
	jz	noodat			;no, start ignoring output

	cmp	di,ds:[wend+po&pn&] 	;see if this is the last output byte
	ja	bout			;handle BREAK output

	mov	al,ss:[di+bp]
	out	scc&pn&+1,al		;output the next data byte
	je	j_oend			;wrap up if that was the last byte
	inc	di
	mov	ds:[wptr+po&pn&],di 	;save new pointer
	jmp	exit

;not ready for break, so check DCD
; We cannot re-fetch RR0 without potentially messing up the TX status bit
nobrk:	mov	al,cl
	and	al,not RR0_TXE

;Output not ready or needed.  Check for DCD change.
;   AL=RR0
noodat:	mov	cl,al
	xor	al,ds:[prvst+po&pn&]
	jnz	dcdint			;(this jump saves a clock)
	jmp	exit


;possible DCD change
dcdint:	test	al,RR0_DCD
	mov	al,cl
	jz	sndst
	or	ch,intbit_dcd		;on a status interrupt,
	mov	si,bx			;force immediate status interrupt

sndst:	mov	ds:[prvst+po&pn&],al
	sar	al,1
	mov	ss:[sfs+do&pn&+bp],al	;send new status to the host
	and	al,80h
	mov	ds:[brkbit+po&pn&],al
	jmp	exit

;handle BREAK output
bout:	mov	cl,al
	test	byte ptr ds:[wr5+po&pn&],10h	;break already active?
	jz	bstrt			;no, go start it
	cmp	bx,ds:[brkclk+po&pn&]	;yes, does the clock say it is done?
	js	nobrk			;no, let the break continue

;end BREAK
	mov	al,5			;reset break bit when finished
	out	scc&pn&,al		;point to wr5
	mov	al,ds:[wr5+po&pn&]
	and	al,0efh			;and clear break bit in SCC
	mov	ds:[wr5+po&pn&],al
	out	scc&pn&,al
j_oend:	jmp	oend&pn&

;start break
bstrt:	mov	al,1h			;point at WR1
	out	scc&pn&,al	
	lea	ax,[bx+250000/clkres]
	mov	ds:[brkclk+po&pn&],ax	;start the timer while waiting for SCC
	in	al,scc&pn&		;fetch RR1 status bits
	test	al,RR1_AS
	jz	nobrk			;delay start until last output done

	mov	al,5			;point to wr5
	out	scc&pn&,al
	mov	al,ds:[wr5+po&pn&]
	or	al,10h			;set send-break bit 
	mov	ds:[wr5+po&pn&],al
	out	scc&pn&,al
j_exit:	jmp	exit
	endm


;end output on a port
oend	macro	pn, exit
	local	oend2

	xor	ax,ax
	xchg	ax,ds:[wptr2+po&pn&]
	mov	ds:[wptr+po&pn&],ax	;mark buffer finished
	or	ax,ax
	jz	oend2			;and quit

	mov	ax,ds:[wend2+po&pn&]	;or start the next one
	mov	ds:[wend+po&pn&],ax
	mov	byte ptr ds:[txon+po&pn&],RR0_TXE ;previous may have been BREAK
	mov	byte ptr ss:[opb+do&pn&+bp],0ffh	;tell the host
	jmp	exit

;here AL=0
oend2:	mov	ss:[opb+do&pn&+bp],al	;tell host output is finished
	mov	ds:[txon+po&pn&],al	;turn off output
	or	ch,intbit_out
	mov	si,bx			;force immediate output interrupt
	jmp	exit
	endm
page
;**********************************************************************
;
cseg	segment	word	public	'code'
	assume	cs:cseg

extrn	prhcmd:near


;	**** Main routine ****
;
; we keep BX=clock, SI=input interrupt delay,  CH=interrupt bits,
;	BP=0, ES,SS=base of dual port SRAM,  DS=base of scratch pad SRAM,
;	AX,BX,CL,DX,DI=scratch

	public	main
mainp	proc	near

;Come here from the initialization code and after a host command.
main:	mov	ax,dpa0
	mov	ss,ax
	mov	es,ax
	mov	bx,ds:[clk]		;restore the common registers
	mov	si,ds:[idlyclk]
	mov	ch,ds:[intbits]
	xor	bp,bp
	cld


;Unroll the main loop to minimize the cost of checking the slow ports,
;but guarantee the slow ports do not starve.
; check ports 	0,1	once		9600
;	   2,3,4,5,6,7	four times	38400

mlop:	add	bx,(215+clkres/2)/clkres ;advance clock by loop overhead

	ckio	0, ckp1a, work0a
ckp1a:	ckio	1, ckp2a, work1a
ckp2a:	ckio	2, ckp3a, work2a
ckp3a:	ckio	3, ckp4a, work3a
ckp4a:	ckio	4, ckp5a, work4a
ckp5a:	ckio	5, ckp6a, work5a
ckp6a:	ckio	6, ckp7a, work6a
ckp7a:	ckio	7, ckp2b, work7a

j_oend2a:jmp	oend2
j_oend3a:jmp	oend3
j_oend4a:jmp	oend4
j_oend5a:jmp	oend5

ckp2b:	fasto	2,j_oend2a,n/a
	fasto	3,j_oend3a,n/a
	fasto	4,j_oend4a,n/a
	fasto	5,j_oend5a,n/a
	fasto	6,j_oend6a,n/a
	fasto	7,  oend7, ckp2c
j_oend6a:jmp	oend6

ckp2c:	fastio	2,j_oend2c,n/a
	fastio	3,  oend3 ,ckp4c
j_oend2c:jmp	oend2
j_oend4c:jmp	oend4
ckp4c:	fastio	4,j_oend4c,n/a
	fastio	5,j_oend5c,n/a
	fastio	6,  oend6 ,ckp7c
j_oend5c:jmp	oend5
j_oend7c:jmp	oend7
j_oend2d:jmp	oend2
ckp7c:	fastio	7,j_oend7c,n/a

	fasto	2,j_oend2d,n/a
	fasto	3,j_oend3d,n/a
	fasto	4,j_oend4d,n/a
	fasto	5,j_oend5d,n/a
	fasto	6,j_oend6d,n/a
	fasto	7,j_oend7d,n/a

	cmp	bx,si			;has the interrupt timer expired?
	jns	ckint			;yes, see what to do

	in	al,rdstat 		;read the 3608 status port
	shl	al,1			;is there a new host command?
	jc	hcmd			;yes, handle it.

	jmp	mlop

j_oend3d:jmp	oend3
j_oend4d:jmp	oend4
j_oend5d:jmp	oend5
j_oend6d:jmp	oend6
j_oend7d:jmp	oend7


ckint:	or	ch,ch			;any interrupts to handle?
	jz	noint			;no, forget it

	cmp	byte ptr ss:[isf+bp],0	;previous interrupt ack'ed by host?
	jne	j_mlop			;no, try later
	
	mov	ss:[isf+bp],ch		;yes, tell host why
	mov	al,ds:[intvec]
	mov	dx,ds:[intlvl]
	out	dx,al			;cause the interrupt
	xor	ch,ch

noint:	mov	si,bx			;no more interrupts for a while
	add	si,ds:[idlyval]
j_mlop:	jmp	mlop


hcmd:	mov	ds:[clk],bx		;save the common registers
	mov	ds:[idlyclk],si
	mov	ds:[intbits],ch
	jmp	prhcmd			;process host command


oend7:	oend	7, ckp2b
oend6:	oend	6, ckp7a
oend5:	oend	5, ckp6a
oend4:	oend	4, ckp5a
oend3:	oend	3, ckp4a
oend2:	oend	2, ckp3a
oend1:	oend	1, ckp2a
oend0:	oend	0, ckp1a

work0a:	svport	0, ckp1a
work1a:	svport	1, ckp2a
work2a:	svport	2, ckp3a
work3a:	svport	3, ckp4a
work4a:	svport	4, ckp5a
work5a:	svport	5, ckp6a
work6a:	svport	6, ckp7a
work7a:	svport	7, ckp2b
	
mainp	endp

cseg	ends
	end
