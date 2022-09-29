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

title	3608SELF	-self test for 23/3608
comment	%
	Test Description:
	This test is seperated into 7 main diagnostics.
	This test forces normal/burn-in mode unlike NUSELF2.
	Each section will handle a specific test on the
	23/3608.  When an error occurs it is reported
	on the LED's and the system will hang in
	most circumstances.  The SCC error code will only
	be displayed if all SCCS have failed.  Here are the
	main diagnostics of the test:
		prom tests
		scratchpad tests
		dual port tests
		bus lock tests
		scc address tests
		data turnaround tests
		interrupt tests
	Some sections are subdivided even further.  Each test
	will have a thorough description of purpose.
	%

include 3608equ.h

;	MACRO's
;Below are all the macro's required to run
;the main program, stest.
;----------------------------------------
;
;       PROM SUMCHECK
;
;Total up all byte in the EPROM (2000h). The
;final amount should equal zero.  This is
;because of the burning of a byte in the final
;prom which is the two's complement of the total.
;If this section fails light error code 02h.
;On failures, repeatedly read the prom.  If
;in single test mode, run this test continuously
;even after reporting errors.
;
prom	macro
;
jprom:	nop
	mov	ax,3000h     ;set extra segment to dual port ram
	mov	es,ax
	xor  bx,bx		;start at address 0
	mov	ax,bx		;clear ax
prom1:	add	al,cs:[bx]	;add in the next byte
	inc	bx			;up to 4000h bytes
	cmp 	bx,4000h
	jnz	prom1
	test	al,0ffh    ;if the total is zero end section
	jz	prom2		;if not zero, report error on LEDs
	out	18h,al	    ;turn  LED 1 off and turn on 2
	out	11h,al
	mov	byte ptr es:00h,01h	;test 1 failed
	mov	byte ptr es:09h,0fh	;load segment & offset of last
	mov	es:00ah,bx	;address into dualport
	mov	byte ptr es:00ch,00h	;and expected sum byte
	mov	es:00dh,al	;and calculated result byte
	cmp	di,01h	  ;if single test mode go repeat this test
	je	prom2
	mov	bx,5555h
prome:	mov	ah,cs:[bx]	;read prom addresses 5555h and
	shl	bx,01h		;AAAAh repeatedly.
	mov	ah,cs:[bx]
	shr	bx,01h
	jmp	prome
prom2:	mov	cl,01h	  ;report one pass in dual port ram
	or	es:008h,cl
	cmp	di,01h	  ;and repeat test if in single test mode
	je	jprom
	endm
;-------------------------------------
;
;	SCRATCHPAD RAM TEST
;
;Do a memory test on 8K of static ram, 2K
;at a time.  A 2K block is written with an
;incrementing pattern and then read to verify
;that the data is correct.  Then the next
;block is tested.  Each block has a differant
;initial byte.  If the data read is incorrect
;an error indication is output onto the LEDs and
;the static ram is repeatedly written and read.
;In single test mode, the test is run continuously
;even after errors are reported.
;
spad	macro
;
jspad:	nop
	mov	ax,000h	  ;set data segment to 00
	mov	ds,ax
	mov	ax,3000h  ;and extra segment to 03
	mov	es,ax
	mov	si,800h
	mov	bx,00h	  ;set address to first location
	mov	ah,01h
	mov	ch,ah
spad1:	mov	ds:[bx],ch    ;write a byte to static ram (sram)
	inc	ch	  ;next data byte, not 00h
	jnz	spad2
	inc	ch
spad2:	inc	bx	  ;next address up to a 2K block
	cmp	bx,si
	jl	spad1
	mov	ch,ah	  ;retrieve first byte this block
	mov	bx,si	  ;calculate first location of this block
	sub	bx,800h
spad3:	mov	cl,ds:[bx]  ;read a byte from this block
	cmp	ch,cl	   ;is it correct?
	jne	spader	  ;no=go report error, yes=continue
spad3a:	inc	ch	  ;get next expected data byte
	jnz	spad4	  ;not 00h
	inc	ch
spad4:	inc	bx	  ;get next address to test, up to end of
	cmp	bx,si	  ;this block
	jl	spad3
	inc	ah	  ;at end of block, increase initial byte
	cmp	ah,04h	  ;for no more than 4 blocks of 2k bytes.
	jg	spad5
	add	si,800h	  ;increase block cutoff to next 2k mark
	mov	ch,ah	  ;load in initial byte
	jmp	spad1	  ;and go test this block
spader:	out	11h,al	;turn on LED 2
	out	18h,al		;and LED 1 off
	mov	byte ptr es:00h,02h	;test 2 failed
	mov	byte ptr es:009h,00h	;write segment number and
	mov	es:00ah,bh      ;offset to dual port ram
	mov	es:00bh,bl
	mov	es:00ch,ch	;write expected and
	mov	es:00dh,cl	;received byte into d.p.
	cmp	di,02h	     ;to dual port ram (d.p.)
	je	spad3a	 ;if single test mode, go continue test
	mov	dx,040h
	mov	bx,5555h  ;initialize i/o addr. to first USART, also
	mov	ah,55h	  ;memory addr. and data to 5's.
spader1: out	dx,al	;output trigger signal
	mov	ds:[bx],ah	;wr. & rd. data at sram location
	mov	al,ds:[bx]
	shl	ah,1h	    ;next data byte (aah)
	out	dx,al	   ;output trigger signal again
	mov	ds:[bx],ah	;wr. & rd. data at sram location
	mov	al,ds:[bx]
	shr	ah,01h	     ;next data byte (55h)
	shl	bx,01h	     ;next address
	jnc	spader1	     ;then return
	mov	bx,5555h
	jmp	spader1
spad5:	mov	cl,02h	    ;set test complete bit in d.p.
	or	es:008h,cl
	cmp	di,02h	   ;if in single test mode, repeat this test
	jne	spad6
	jmp	jspad	   ;endlessly else exit this section.
spad6:	nop
	endm
;-----------------------------------------------------
;
;	DUAL PORT RAM TEST
;
;Run a memory test on 64k of dual port ram.  Write
;a pattern to all ram and verify.  Repeat 4 times
;incrementing the first byte to write.  The pattern
;will start at 01h and inc. by byte to ffh and then
;repeat at 01h.  The second pass should start at 02h
;and run to ffh then repeat at 01h  etc.
;In burn-in mode, the first 0fh bytes are not tested
;after the first pass of this whole program.  If the
;data read is not the correct pattern, an error indication
;is output to the LEDs and the ram is continually written
;and read.  In single test mode the test is run repeatedly
;even after errors are reported.
;
dual	macro
;
jdual:	nop
	mov	ax,3000h
	mov	ds,ax      ;initialize data segment to 3
	mov	dh,01h    ;set first data byte mark to 01h
dual1:	mov	bx,0000h   ;initialize base ptr to first byte of ram
	mov	ah,dh     ;get this passes first byte (mark)
	cmp	bp,00h	  ;is this first pass? yes, bypass adjuster
	jz	dual2	  ;no, then don't write over first 0fh bytes
	mov	bx,010h	  ;first byte location after first pass
	mov	ah,014h	  ;first data byte after first pass
dual2:	mov	ds:[bx],ah   ;write data to location
	inc	bx
	jz	dual3      ;next location and next data, not 00h
	inc	ah	  ;that is, do 64k locations.
	jnz	dual2	  ;next data, not 00h
	inc	ah
	jmp	dual2
dual3:	pass		  ;if after first pass exit on dummp command
	mov	ah,dh     ;get mark byte and read data pattern in ram
	mov	bx,0000h   ;set ptr to first location
	cmp	bp,00h	  ;is this first pass?
	jz	dual4	  ;yes, then bypass adjustments
	mov	bx,010h	  ;no, then don't test first 0fh bytes
	mov	ah,014h   ;force first data byte
dual4:	mov	al,ds:[bx]  ;get data byte
	cmp	ah,al      ;is data = that written
	jne	dual6      ;no, then go handle error
dual4a:	inc	bx
	jz	dual5    ;next location up to 64k
	inc	ah
	jnz	dual4   ;next data, but not 00h
	inc	ah
	jmp	dual4
dual5:	pass	      ;after first pass, jmp to drv on new command
	inc	dh   ;next pass. inc. mark (first byte next pass)
	cmp	dh,04h   ; up to four passes
	jle	dual1
	jmp	dual7
dual6:	out	18h,al		;turn LED 1 off
	out	11h,al	  ;and turn LED 2 ON
	mov	byte ptr es:00h,03h	;test 3 failed
	mov	ds:009h,dh	;put failed address in D.P.
	mov	ds:00ah,bh
	mov	ds:00bh,bl
	mov	ds:00ch,ah	;put exp. data & rec. data in D.P.
	mov	ds:00dh,al
;	jmp	dual4a		;this step is strictly for testing dp read
	cmp	di,03h	  ;if in single test mode, continue test
	je	dual4a
	mov	bx,05555h	;set base address & data
	mov	dx,040h
	mov	ah,055h
duale:	out	dx,al		;write to  reg of USART 0
	mov	ds:[bx],ah	;wr to 5555h or aaaah a 55h
	mov	al,ds:[bx]	;rd same address
	shl	ah,1
	out	dx,al		;wr to reg of scc 0 again
	mov	ds:[bx],ah	;wr to same d.p. address an aah
	mov	al,ds:[bx]	;read same addr.
	shr	ah,1
	shl	bx,1		;adjust data back to 55h
	jnc	duale		;and address to next, aaaah or 5555h
	mov	bx,05555h
	jmp	duale
dual7:	cmp	di,03h	   ;if in single test mode, repeat the test
	jne	duala7
	cmp	bp,00h
	jne	dua7a
	mov	bx,00h
dua7:	mov	word ptr ds:[bx],00h
	add	bx,02h
	cmp	bx,10h
	jl	dua7
	or	byte ptr ds:008h,04h   ;set pass bit in d.p.
dua7a:	inc	bp
	jmp	jdual
duala7:	cmp	bp,00h	  ;if first pass of program, initialize
	jnz	dual7b	  ;the first 0fh bytes of d.p. to 00h
	mov	bx,00h
dual7a:	mov	word ptr ds:[bx],00h
	add	bx,02h
	cmp	bx,10h
	jl	dual7a		;and set tests passed bits to 3
	or	byte ptr ds:008h,07h
	jmp	dual7c
dual7b:	or	byte ptr ds:008h,04h  ;set dual pass bit
dual7c:	nop
	endm		 ;end section if no errors
;------------------------------------------------
;	SCC ADDRESS DECODE TEST
;
;Write a unique byte to a register of each
;channel.  Save the byte read in an eight byte
;buffer.  Reread all command registers and compare
;them to the saved byte.  Set error bit 0 in dual port
;bytes 0-7 when an error occurs and report
;a non-fatal error in dual-port.  If all SCCs fail, report
;a fatal error by lighting the red LED and do repeated writes and
;reads of SCCs.  Repeat test in single mode even
;after fatal errors.
;
udec	macro
;
judec:	nop
	mov	ax,3000h   ;set es to seg 3
	mov	es,ax
	mov	ax,00h
	mov	ds,ax      ;set ds to seg 0
	sub	ax,ax	   ;clear ax
	mov	bx,20h	   ;first byte of buffer
udec1:	mov	ds:[bx],ax   ;clear 8-byte buffer
	add	bx,02h   ;in scratchpad
	cmp	bx,28h
	jl	udec1
	mov	bx,ax		;clear bx
udec1a:	cmp	bp,00h	  ;don't clear d.p. if second pass of program
	jnz	udec1c
udec1b:	mov	es:[bx],ax  ;clear first 8 bytes of dual port
	add	bx,02h
	cmp	bx,08
	jl	udec1b
udec1c: 	mov	cx,20h		;set to initial buffer byte
	mov	dx,40h			;port0 address
udec2:	mov	al,0ch   			;write to register 0 to indicate register
	out	dx,al			;to be written (0c=register 12)
	nop
	inc 	ah				;get unique byte
	mov	al,ah			;and copy into al
	out 	dx,al			;write unique byte to register (12)
	nop
	nop
	nop
	mov	al,0ch			;must write to reg 0 each time to
	out	dx,al			;indicate reg to be accessed
	nop
	nop
	nop
	nop
	in	al,dx			;read register 12
	cmp	ah,al			;does byte read = byte written?
	je	udec5			;yes--store byte
	mov	al,00h			;no--store 00h
udec5:	mov	bx,cx
	mov	ds:[bx],al		;store byte or 00h
	inc	cx
	add	dx,08h			;addr of next port
	cmp	dx,80h			;done?
	jl	udec2			;repeat until all 8 ports
	mov	dx,40h			;return to addr of 1st port
	mov	bx,20h			;restore bx to point to 1st byte
udec6:	mov	al,0ch			;0c points to register 12
	out	dx,al			;write to reg 0 to tell reg# to be accessed
	nop
	nop
	nop
	in	al,dx			;read register 12
	cmp	ds:[bx],al		;does byte read = byte stored?
	jne	udec7			;no--handle error
	cmp	al,00h			;else does byte read = 0?
	jne	udec8			;no--this port is not in error
udec7:	mov	si,bx	     ;on errors save bx
	and	bx,0fh 			;change offset from 2x to 0x
	or	byte ptr es:[bx],01h	;write error code of 01h in buffer
	mov	bx,si			;restore bx
udec8:	add	dx,08h		;next port
	inc	bx
	cmp 	dx,80h			;check if last port
	jl	udec6			;if not, continue
	mov	bx,00h			;restore bx to point to 1st buffer
udec9:	test	byte ptr es:[bx],01h  ;check each port for a pass condition
	jz	udec9a			;if one port passed, non-fatal error
	inc	bx				;point to next buffer
	cmp	bx,08h			;was that the last port?
	jl	udec9			;no then check next port
	out	18h,al	  ;output error code
	out	11h,al	  ;i.e. all channels failed.
	mov	byte ptr es:00h,04h	;test 4 failed
	cmp	di,06h	  ;if single mode repeat test
	jne	udeca9
	or	byte ptr es:08h,10h  ;set pass bit in dual port
	jmp	judec
udeca9:	wrus		  ;else do repeated writes and reads to Usarts
udec9a:	or	byte ptr es:008h,10h  ;set one pass bit in dual port
	cmp	di,06h
	jne	udec9b
	jmp	judec	 ;repeat test if in single mode
udec9b:	nop
	endm

;----------------------------------------------
;
;	WRITE AND READ USARTS
;
;Runs continuously by writing and reading to all
;USARTS. This test will also run if all eight
;ports can't be addressed.
;
wrus	macro
	local	wrus0,wrus1,wrus2,wrus3,wrus4,wrus4a,wrus6,wrus7

wrus0:	mov	dx,040h		;1st scc
wrus1:	mov	al,0ch		;wr12
	out	dx,al
	nop
	nop
	nop
	mov	al,55h		;data written = 55
	out	dx,al
	cmp	dx,078h		;done ?
	je 	wrus2		;yes continue
	add	dx,08h		;else write next port
	jmp	wrus1
wrus2:	mov 	dx,40h		;return to 1st port to read byte
wrus3:	mov	al,0ch		;must point to wr12 again
	out	dx,al
	nop
	nop
	nop
	in	al,dx		;read data
	cmp	al,55h		;55 was written
	jne	wrus7		;if not eq report error
wrus4:	cmp	dx,78h	;do ports 50,58,60,68,70,78
	je	wrus0
	add	dx,08h
	jmp	wrus3

wrus7:	out	18h,al	;error - turn off led 1
	out	11h,al		;and turn on led 2
	jmp	wrus4
	endm

;---------------------------------------------
;
;	END OF PROGRAM
;
;This section will decide whether to repeat
;this program or pass control to the driver.
;After the first pass, the busy bit is
;cleared and each subsequent pass will toggle
;LED 1.
;
tend	macro
;
	cmp	bp,00h	 ;is this the first pass?
	jnz	tend1	 ;no, then jump
	out	30h,al	 ;else set ready bit
	out	18h,al	 ;and turn off LED 1
	mov	ax,3000h
	mov	ds,ax
	inc	bp	 ;inc # of passes in d.p.
	mov	bx,bp
	mov	ds:00eh,bh
	mov	ds:00fh,bl  ;and repeat program
	jmp	norm1
tend1:	in	al,20h	 ;is there a new command?
	test	al,80h
	jnz	tend4	 ;yes,then go prepare for end
	test	bp,01h	 ;else turn off LED 1
	jnz	tend2	 ;when bp is an even #
	out	18h,al	 ;or turn LED 1 on when
	jmp	tend3	 ;bp is an odd #
tend2:	out	10h,al
tend3:	inc	bp	 ;inc the loop count
	mov	ax,3000h  ;then store the loop count(bp)
	mov	ds,ax	  ;in d.p.
	mov	bx,bp
	mov	ds:00eh,bh
	mov	ds:00fh,bl
	jmp	norm	  ;and repeat test

tend4:	out	10h,al	  ;turn led 1 on and go to the
	jmp	istart	  ;main driver
	endm

;-------------------------------------------
;
;	Control Processor
;
;After the first pass of this program, any
;dummy new command will allow this section
;to jump to the main driver.
;
pass	macro
	local	pass1
;
	cmp	bp,00h
	jz	pass1
	in	al,20h	  ;is there a new command
	test	al,80h	  ;bd7=1?
	jz	pass1	  ;no, then continue program
	out	10h,al	  ;else turn LED 1 on
	jmp	istart	  ;and jump to main driver
pass1:	nop
	endm
;-------------------------------------------
;
;	INITIALIZATION
;
;turn LED's OFF by writing to i/o 18-19h.
;set up the stack in scratchpad memory at
;segment 0000h, with the pointer at 7FFh.
;Set the data segment to 0000h. Clear error
;register DI.
;
cseg	segment	page public 'code'
	assume	cs:cseg
	extrn	istart:near

stest	proc	near
	mov	di,0fh		;if mode is 00 or 0fh run normal mode

	mov	bp,00h	 ;clear loop counter
	out	19h,al
	out	10h,al	 ;turn LEDs to 01h
	mov	ax,3000h
	mov	ds,ax
	mov	bx,00h
	mov	ax,00h
stesta:	mov	ds:[bx],ax  ;initialize first 0fh bytes of d.p.
	add	bx,02h
	cmp	bx,10h
	jl	stesta

	initscc

	mov	dx,40h		;check for spurious input characters
stestb: mov	al,05h		;and turn off DTR to reset attached modems
	out	dx,al		;point to wr5
	mov	al,068h		;8 bits\Tx char,Tx en,no RTS
	nop
	nop
	out	dx,al		;load data into reg
	nop	
	nop
	mov	al,03h
	out	dx,al		;point to wr3
	nop
	nop
	nop
	mov	al,0c0h		;8 bits\Rx char, Rx off
	out	dx,al

	in	al,dx		;read reg 0
	test	al,01h		;is there a rx char?
	jz	stestc    	;no- then test next scc
	inc	dx			;yes- read it
	in	al,dx
	dec	dx
	jmp	stestb		;check for more
stestc:	cmp	dx,78h	;done ?
	je 	norm		;yes- goto end
	add	dx,08h		;no- then check next
	jmp	stestb
;-----------------------------------------------
;
;	Normal or Main Section
;
;Run all main sections of the test as macros.
;
norm:	mov	ax,7FFh
	mov	sp,ax

;this is the main section of the program, run as macros
norm1:	prom		;run prom sumcheck
;
	pass	 	;control decision, test or driver?
;
	spad		;test scratchpad ram (move stack)
;
	pass		;control decision
;
	dual		;test dual port ram memory
;
	pass		;control decision
;
	udec		;test usart address decode
;
	pass		;control decision
;
	tend		;control processor
;
stest	endp
;
cseg	ends
	end
