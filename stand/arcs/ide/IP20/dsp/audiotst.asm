;
;	audiotst.asm -- dsp audio test code
;	dsp code starts, then writes $ffffff to y:$8020
;	and loops until y:$8020 changes (host will write test number to it)
;	y:$8020 is then read and interpreted to be a test to be run
;	y:$8021 is the frequency increment to be use
;	y:$8022 is the input gain to be used
;	y:$8023 is the output gain to be used
;	y:$8024 is the sin divisor to be used
;	y:$8025 is the transmit sample rate frequency to be used
;	y:$8026 is the receiver sample rate frequency to be used
;	y:$8027 is number of internal loops to do
;	y:$8028 is value of leftc for analog and mic tests
;	y:$8029 is value of rightc for analog and mic tests
;	When done $ffffff is written to y:$8020 to show test done
;	y$8020 is $ffffff to show host test is done
;	y:$8014 is error number ($ffffff=no errors)
;
;	y:$c200 is receiversample rate clock buffer (1024 words)
;	y:$c400 is transmit sample rate clock buffer (1024 words)
;
	section	Initialization
	include 'hdsp.inc'
;
; Errors reported back to host in errnum (y:$8104)
DTXERR		equ	0		; digital transmitter error
DRXERR		equ	1		; digital receiver error
ATXERR		equ	2		; analog transmitter error
ARXERR		equ	3		; analog receiver error
SFIERR		equ	4		; soft interrupt error
SFIBERR		equ	5		; soft bit interrupt error 
RBIERR		equ	6		; receive block interrupt error
RBIBERR		equ	7		; receive block bit interrupt error 
TBIERR		equ	8		; transmit block interrupt error
TBIBERR		equ	9		; transmit block bit interrupt error 
RNAIERR		equ	10		; receive nonaudio interrupt error
RNAIBERR	equ	11		; receive nonaudio bit interrupt error 
TNAIERR		equ	12		; transmit nonaudio interrupt error
TNAIBERR	equ	13		; transmit nonaudio bit interrupt error 
IRQERR		equ	14		; irq error

BUFSIZE		equ	8192		; buffer size (power of 2)

; Here it is - The hardware entry point into the DSP microcode! This is the
; first instruction executed when the Hollywood takes the DSP out of reset.
	org	p:$e000
	movec	#0,sp			; clear stack
;
; NOTE : x0 is always=1 so everyone can use it for decrementing
	move	#>1,x0
;
; Initialize the system.
	movep	#>$4,x:<<BCR		; 4 wait states per I/O access
	movec	#<6,omr			; enable rom
	movep	#00,y:DMACTRL
	movep	#00,y:INTMASK	
	movep	#$3e1,y:INTCONF	
	movep	#00,y:INTPOL	
	movep	#00,y:INTSTAT	
	movep	#08,y:MISCCSR		;32K SRAM

; HPC1 Bug Workaround.  See Rick Jeng or Kishore Gottimukkala for details.
	movep   #$4100,y:BURSTCTL

	movep	#$0,x:PBC		;Configure Port b
	movep	#$7fff,x:PBDDR

	movep	#$000,x:PCC		;Make port c be the ssi
	movep	#$6000,x:CRA		;Configure the ssi.
	movep	#$0100,x:CRB		; turn off rx/tx
	movep	#$1F8,x:PCC		;Make port c be the ssi

	movep	#$0,y:HEADPHONE_MDAC_L
	movep	#$0,y:HEADPHONE_MDAC_R

	movep	#$6200,x:PBD		; Reset the AES receiver and nogo
	movep	#$2200,x:PBD
	movep	#$6200,x:PBD

	movep	#$60fe,x:PBD
	movep	#$20fe,x:PBD
	movep	#$60fe,x:PBD
	movep	#$61fc,x:PBD
	movep	#$21fc,x:PBD
	movep	#$61fc,x:PBD
	movep	#$61fe,x:PBD		; calibrate the ADC
	movep	#$21fe,x:PBD
	movep	#$61fe,x:PBD

        movep   #$7f00,x:PBDDR		; Prepare to read the revision level
        movep   #$6500,x:PBD
        movep   #$4500,x:PBD

; Find out how fast y:COUNT (The GIO-Bus clock) is.  We do this by reading
; it before and after a 100,000 instruction loop, taking the difference
; and dividing by 10 (for better accuracy).  The loop takes 1/10 second to
; complete.
	clr	a
	clr	b	#49998,a1
	nop			;Just for the hell of it, drain the pipeline.
	nop
	nop
	nop

;	*** Start Timing here ***
	movep	y:COUNT,b1	;b=start time
;	0 instructions.
	rep	a
	nop
;	50000 instructions.
	rep	a
	nop
;	100000 instructions.
	movep	y:COUNT,a1	;a=end time
;	*** End Timing here ***

	move	#0,a2
	move	#0,a0
	sub	b,a
	move	#0,a2
	move	#0,a0		;a=time difference
	move	a1,y:cp10ms	;CountsPerTenMilliseconds

        movep   x:PBD,a1		; Read the revision level
	move	a1,y:revlev
        movep   #$6500,x:PBD
        movep   #$7fff,x:PBDDR
;
; write sin table to ram for mips to read
	move	#>$100,r7
	move	#>$ff,m7
	move	#sintab,r6
	move	#>$ffff,m6		; sintab is only 256
	move	#>$400,r1		; but we'll write four copies so the
					; mips can wrap through it
	do	r1,copysine
	move	y:(r7)+,a		; copy from sin table to sintab
	move	a1,y:(r6)+
copysine:
	movep	#$6C00,x:PBD		;Misc xmit control
	movep	#$2C00,x:PBD
	movep	#$6C00,x:PBD
;
; dispatch loop comes back here
dispatcher:
	movec	#0,sp			; clear stack
	movep	#$0,y:HEADPHONE_MDAC_L
	movep	#$0,y:HEADPHONE_MDAC_R

	movep	#$6900,x:PBD		;Disable A/D Converter
	movep	#$2900,x:PBD
	movep	#$6900,x:PBD

	movep	#$6a80,x:PBD		;Don't use diag mode, use 48 clock, nogo
	movep	#$2a80,x:PBD
	movep	#$6a80,x:PBD

	movep	#$6207,x:PBD		;Unreset the AES receiver and nogo
	movep	#$2207,x:PBD
	movep	#$6207,x:PBD

	movep	#$000,x:PCC		;Make port c be the ssi
	movep	#$6000,x:CRA		;Configure the ssi.
	movep	#$0100,x:CRB		; turn off rx/tx
	movep	#$1F8,x:PCC		;Make port c be the ssi

	move	#>$ffffff,a		; tell host we got here
	move	a1,y:hshake

	move	#>10000,a		; let receiver/transmitter stop
wtloop2:
	sub	x0,a
	jne	wtloop2
;
; host wait loop
dispwait:
	move	y:hshake,a		; loop waiting for host data
	tst	a
	jlt	dispwait

	move	a1,n0			; use host data as indirect jump
;
; set up constants
	move	#>$4000,y1		; to reverse strobe 
;
; set up left and right gain settings in PBD triplet
	move	y:ingain,a		; gain from host
	asl	a
	asl	a
	move	a,y0			; save as shifted version
	move	#>$6002,a		; register 0, mic in, hi strobe
	or	y0,a			; add shifted gain
	move	a1,y:lftgmh		; left gain mic hi strobe
	eor	y1,a			; reverse strobe
	move	a1,y:lftgml		; left gain mic lo strobe

	move	#>$6001,a		; register 0, line in, hi strobe
	or	y0,a			; add shifted gain
	move	a1,y:lftglh		; left gain line hi strobe
	eor	y1,a			; reverse strobe
	move	a1,y:lftgll		; left gain line lo strobe

	move	#>$6100,a		; register 1, hi strobe
	or	y0,a			; add shifted gain
	move	a1,y:rtgh		; right gain hi strobe
	eor	y1,a			; reverse strobe
	move	a1,y:rtgl		; right gain lo strobe

	movep	y:rtgh,x:PBD		;Set ADC right gain
	movep	y:rtgl,x:PBD
	movep	y:rtgh,x:PBD
;
; set up sample rate frequency settings in PBD triplet
	move	y:txsrf,a		; txsrf from host
	asl	a
	move	#>$6a81,y0		; register 10, adc normal, hi strobe
	or	y0,a			; add shifted txsrf
	move	a1,y:txctrh		; transmit control hi strobe
	eor	y1,a			; reverse strobe
	move	a1,y:txctrl		; transmit control lo strobe

	move	y:txsrf,a		; txsrf from host
	asl	a
	move	#>$6a80,y0		; register 10, adc normal, hi strobe
	or	y0,a			; add shifted txsrf
	move	a1,y:txctrofh		; transmit control hi strobe
	eor	y1,a			; reverse strobe
	move	a1,y:txctrofl		; transmit control lo strobe
;
; set up recevier frequency settings in PBD triplet
	move	y:rxsrf,a		; rxsrf from host
	asl	a
	move	#>$6901,y0		; register 9, adc normal, hi strobe
	or	y0,a			; add shifted txfreq
	move	a1,y:rxctrh		; receiver control hi strobe
	eor	y1,a			; reverse strobe
	move	a1,y:rxctrl		; receiver control lo strobe

	move	y:volume,a		; set volume
	movep	a1,y:HEADPHONE_MDAC_L
	movep	a1,y:HEADPHONE_MDAC_R
	move	#bufsav,r2		; buffer for collecting data
	move	#>$ffff,m2
	move	#>$100,r3		; sin table index
	move	y:frqinc,n3		; get frequency from host
	move	#$ff,m3			; limit sin table to 100-1ff
	move	#txclk,r4		; transmit sample rate clock buffer
	move	#>$1ff,m4		; limit to 512
	move	#rxclk,r5		; receiver sample rate clock buffer
	move	#>$1ff,m5		; limit to 512
	move	#>$ffff,m0
	move	#>dispatchtab,r0
	move	#>$ffffff,a
	move	y:(r0+n0),r0
	move	a1,y:errnum		; show no errors
	clr	a
	move	a1,y:sendside		; set flag to show sent left
	move	#>BUFSIZE,b		; count for buffer
	jmp	(r0)

Test0:
	jsr	digtxrx			; digital tx/rx test
	jmp	dispatcher
Test1:
	jsr	anltxrx			; analog tx/rx test
	jmp	dispatcher
Test2:
; note: jmp because of interrupt processing
	jmp	actltst			; Actel functional test
Test3:
	jsr	digtst			; direct digital test
	jmp	dispatcher
Test4:
	jsr	anltst			; analog level test
	jmp	dispatcher
Test5:
	jsr	mictst			; microphone level test
	jmp	dispatcher
Test6:
	jsr	digtst			; direct digital sample rate test
	jmp	dispatcher
Test7:
	jsr	anltst			; analog level sample rate test
	jmp	dispatcher
Test8:
	jsr	anltst			; analog input gain test
	jmp	dispatcher
Test9:
	jsr	mictst			; microphone input gain test
	jmp	dispatcher
Test10:
	jsr	mictst			; microphone output gain test
	jmp	dispatcher
Test11:
	jsr	digtst			; digital level test
	jmp	dispatcher
Test12:
	jsr	digplay			; digital loop through
	jmp	dispatcher
Test13:
	jsr	anlplay			; analog loop through
	jmp	dispatcher
Test14:
	jsr	micplay			; microphone loop through
	jmp	dispatcher
Test15:
	jsr	sinplay			; sine wave out 
	jmp	dispatcher
;************************************************************************
;
; Test 0
; Digital transmitter/receiver test set up
;************************************************************************
digtxrx:
	movep	a1,x:TX			;Pre-stuff one sample into ssi tx.
	movep	#$3100,x:CRB		; enable tx and rx ssi
	movep	y:txctrh,x:PBD		; go
	movep	y:txctrl,x:PBD
	movep	y:txctrh,x:PBD

	move	#>$fffff,a
digtx0:
	jset	#6,x:SSISR,digtx1
	sub	x0,a
	jne	digtx0
;
; error, transmitter never responded
	move	#>DTXERR,a		; report error
	move	a1,y:errnum		; tell host error number
	rts
;
; transmitter works, try receiver
digtx1:
	movep	#$6217,x:PBD		;Unreset the AES receiver and go
	movep	#$2217,x:PBD
	movep	#$6217,x:PBD

	move	#>$fffff,a
digrx0:
	jset	#7,x:SSISR,digrx1
	sub	x0,a
	jne	digrx0
;
; error, receiver never responded
	move	#>DRXERR,a		; report error
	move	a1,y:errnum		; tell host error number
	rts
;
; receiver works, continue
digrx1:
	movep	x:RX,a			; get word received
	rts

;************************************************************************
;
; Test 1
; Analog transmitter/receiver test
;************************************************************************
anltxrx:
	movep	y:lftglh,x:PBD		;Set ADC left gain and left anl in
	movep	y:lftgll,x:PBD
	movep	y:lftglh,x:PBD

	movep	a1,x:TX			;Pre-stuff one sample into ssi tx.
	movep	#$3100,x:CRB		; enable tx and rx ssi
	movep	y:txctrh,x:PBD		; go
	movep	y:txctrl,x:PBD
	movep	y:txctrh,x:PBD
	move	#>$7fffff,a
anltx0:
	jset	#6,x:SSISR,anltx1
	sub	x0,a
	jne	anltx0
;
; error, transmitter never responded
	move	#>ATXERR,a		; report error
	move	a1,y:errnum		; tell host error number
	rts
;
; transmitter works, try receiver
anltx1:
	movep	y:rxctrh,x:PBD		;Enable the A/D Converter
	movep	y:rxctrl,x:PBD
	movep	y:rxctrh,x:PBD

	move	#>$7fffff,a
anlrx0:
	jset	#7,x:SSISR,anlrx1
	sub	x0,a
	jne	anlrx0
;
; error, receiver never responded
	move	#>ARXERR,a		; report error
	move	a1,y:errnum		; tell host error number
	rts
;
; receiver works, continue
anlrx1:
	movep	x:RX,a			; get word received
	rts

;************************************************************************
;
; Test 2
; Actel functional test 
;************************************************************************
actltst:
; Set up the IRQB interrupt vector
	move	p:irqbvec,a1
	move	a1,p:IRQB
	move	p:irqbvec+1,a1
	move	a1,p:IRQB+1
	movep	#IRQB_INT,x:IPR		;Enable IRQB interrupts
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; 
; Test Actel soft interrupt
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
	movep	#$670f,x:PBD		; Enable Actel soft interrupts
	movep	#$270f,x:PBD
	movep	#$670f,x:PBD
	movep	#$681f,x:PBD		; Clear Actel interrupts
	movep	#$281f,x:PBD
	movep	#$681f,x:PBD
	movep	#$6800,x:PBD		; Cause Actel soft interrupts
	movep	#$2800,x:PBD
	movep	#$6800,x:PBD

	move	#>$face,a			; clear interrupt stuff
	move	a,y:intreg
	move	a,y:intcnt
	move	#>$ffffff,a		; set a to all 1's
	move	#>100000,b		; set loop count
	andi	#00,mr			; enable interrupts
intloop1:
	tst	a
	jge	intcont1		; go if interrupt wakes us
	sub	x0,b
	jne	intloop1		; keep waiting for interrupt
;
; Error - actel soft interrupt never happened, report it
	move	#>SFIERR,a		; report error
	move	a1,y:errnum		; tell host error number
	jmp	end_irq
;
; Received interrupt, check for actel soft type 
intcont1:
	ori	#03,mr			; disable interrupts
	move	#>$10,x1			; check for actel soft bit
	and	x1,a1
	jeq	intcont2		; ok, continue
;
; Error - actel soft interrupt bit missing, report it
	move	#>SFIBERR,a		; report error
	move	a1,y:errnum		; tell host error number
	jmp	end_irq
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; 
; Test Actel transmit block interrupt 0,1 (block & nonaudio data)
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
intcont2:
	move	#>8,a			; loop count
intloop8:
	move	a1,y:intlpcnt
	move	#>8,a			; loop count
intloop11:
	move	a1,y:intlpcnt2
	movep	#$670b,x:PBD		; Enable transmit block interrupts
	movep	#$270b,x:PBD
	movep	#$670b,x:PBD
	movep	#$681f,x:PBD		; Clear Actel interrupts
	movep	#$281f,x:PBD
	movep	#$681f,x:PBD

	move	#>$cede,a			; clear interrupt stuff
	move	a,y:intreg
	move	a,y:intcnt

	move	y:(r3),a		; get a value (left or right)
	movep	a1,x:TX			; and send it
	movep	#$3100,x:CRB		; enable tx and rx ssi
	movep	y:txctrh,x:PBD		; go
	movep	y:txctrl,x:PBD
	movep	y:txctrh,x:PBD

	movep	#$621f,x:PBD		;Unreset the AES receiver and go
	movep	#$221f,x:PBD
	movep	#$621f,x:PBD

	move	#>$ffffff,a		; set a to all 1's
	move	#>100000,b		; set loop count
	andi	#00,mr			; enable interrupts
intloop5:
	tst	a
	jge	intcont8		; go if interrupt wakes us
	sub	x0,b
	jne	intloop5		; keep waiting for interrupt
;
; Error - actel block interrupt never happened, report it
	move	#>TBIERR,a		; report error
	move	a1,y:errnum		; tell host error number
	jmp	end_irq
;
; Received interrupt, check for actel block type 
intcont8:
	ori	#03,mr			; disable interrupts
	move	#>$4,x1			; check for actel block bit
	and	x1,a1
	jeq	intcont9		; ok, continue
;
; Error - actel block interrupt bit missing, report it
	move	#>TBIBERR,a		; report error
	move	a1,y:errnum		; tell host error number
	jmp	end_irq
;
; Process non-audio data (3 interrupts worth)
intcont9:
	movep	#$6707,x:PBD		; Enable transmit nonaudio interrupts
	movep	#$2707,x:PBD
	movep	#$6707,x:PBD
	movep	#$681f,x:PBD		; Clear Actel interrupts
	movep	#$281f,x:PBD
	movep	#$681f,x:PBD

	move	#>$1234,a
	move	a,y:intreg		; clear interrupt stuff
	move	a,y:intcnt

;
; C bits (mode 00) -- reset sample address clock
	movep	#$6c40,x:PBD		; write non-audio type 00
	movep	#$2c40,x:PBD
	movep	#$6c40,x:PBD
	move	#>20000,b		; set loop count
intloop6:
	sub	x0,b			; wait for 2 ms
	jne	intloop6

	movep	#$6c00,x:PBD		; write non-audio type 00
	movep	#$2c00,x:PBD
	movep	#$6c00,x:PBD
	nop

	btst	#0,y:intlpcnt		; consumer mode?
	jcs	intcont16		; no, continue with broadcast
;
; byte 0
; bit 0		0=consumer, 1=broadcast
; bit 1		0=normal audio mode, 1=non-audio mode
; bit 2-4	emphasis (default=000)
; bit 5		0=sample lock, 1=unlocked
; bit 6-7	sample frequency (default=00=48Khz)
; consumer mode, low bit not set
	movep	#$6b00,x:PBD		; write four times to reg 11
	movep	#$2b00,x:PBD		; low bit determines consumer/broad.
	movep	#$6b00,x:PBD
	jmp	intcont17
;
; broadcast mode, low bit set
intcont16:
	movep	#$6b01,x:PBD		; write four times to reg 11
	movep	#$2b01,x:PBD		; low bit determines consumer/broad.
	movep	#$6b01,x:PBD
intcont17:
	nop
;
; byte 1
; bits 0-3	encoded channel mode (default=000)
; bits 4-7	encoded user bits management
	movep	#$6b00,x:PBD		; write four times to reg 11
	movep	#$2b00,x:PBD
	movep	#$6b00,x:PBD
	nop
;
; byte 2
; bits 0-2	auxiliary sample bits (default=000)
;		(000=20 bits, 001=24 bits, 010=20 bits + aux)
; bits 3-5	sample word length (default = 000)
; bits 6-7	reserved (default=00)
	movep	#$6b23,x:PBD		; write four times to reg 11
	movep	#$2b23,x:PBD
	movep	#$6b23,x:PBD
	nop
;
; byte 3
; bits 0-7	multichannel (default=000)
	movep	#$6b45,x:PBD		; write four times to reg 11
	movep	#$2b45,x:PBD
	movep	#$6b45,x:PBD
	nop

	move	#>$ffffff,a		; set a to all 1's
	move	#>100000,b		; set loop count
	andi	#00,mr			; enable interrupts
intloop7:
	tst	a
	jge	intcont10		; go if interrupt wakes us
	sub	x0,b
	jne	intloop7		; keep waiting for interrupt
;
; Error - actel block interrupt never happened, report it
	move	#>TNAIERR,a		; report error
	move	a1,y:errnum		; tell host error number
;
; Received interrupt, check for actel nonaudio type 
intcont10:
	ori	#03,mr			; disable interrupts
	move	#>$8,x1			; check for actel nonaudio bit
	and	x1,a1
	jeq	intcont11		; ok, continue
;
; Error - actel nonaudio interrupt bit missing, report it
	move	#>TNAIBERR,a		; report error
	move	a1,y:errnum		; tell host error number
	jmp	end_irq
;
; Get and store nonaudio data
intcont11:
	move	#>$feed,a
	move	a,y:intreg		; clear interrupt stuff
	move	a,y:intcnt

;
; U bits (mode 01)
	movep	#$6c01,x:PBD		; write non-audio type 01
	movep	#$2c01,x:PBD
	movep	#$6c01,x:PBD
	nop
	movep	#$6b01,x:PBD		; write four times to reg 11
	movep	#$2b01,x:PBD
	movep	#$6b01,x:PBD
	nop
	movep	#$6b23,x:PBD		; write four times to reg 11
	movep	#$2b23,x:PBD
	movep	#$6b23,x:PBD
	nop
	movep	#$6b45,x:PBD		; write four times to reg 11
	movep	#$2b45,x:PBD
	movep	#$6b45,x:PBD
	nop
	movep	#$6b67,x:PBD		; write four times to reg 11
	movep	#$2b67,x:PBD
	movep	#$6b67,x:PBD
	nop

	move	#>$ffffff,a		; set a to all 1's
	move	#>100000,b		; set loop count
	andi	#00,mr			; enable interrupts
intloop9:
	tst	a
	jge	intcont12		; go if interrupt wakes us
	sub	x0,b
	jne	intloop9		; keep waiting for interrupt
;
; Error - actel block interrupt never happened, report it
	move	#>TNAIERR,a		; report error
	move	a1,y:errnum		; tell host error number
;
; Received interrupt, check for actel nonaudio type 
intcont12:
	ori	#03,mr			; disable interrupts
	move	#>$8,x1			; check for actel nonaudio bit
	and	x1,a1
	jeq	intcont13		; ok, continue
;
; Error - actel nonaudio interrupt bit missing, report it
	move	#>TNAIBERR,a		; report error
	move	a1,y:errnum		; tell host error number
	jmp	end_irq
;
; Get and store nonaudio data
intcont13:
	move	#>$dead,a
	move	a,y:intreg		; clear interrupt stuff
	move	a,y:intcnt

;
; General Purpose Control bits (mode 10)
	movep	#$6c02,x:PBD		; write non-audio type 02
	movep	#$2c02,x:PBD
	movep	#$6c02,x:PBD
	nop
	movep	#$6b89,x:PBD		; write four times to reg 11
	movep	#$2b89,x:PBD
	movep	#$6b89,x:PBD
	nop
	movep	#$6bab,x:PBD		; write four times to reg 11
	movep	#$2bab,x:PBD
	movep	#$6bab,x:PBD
	nop
	movep	#$6bcd,x:PBD		; write four times to reg 11
	movep	#$2bcd,x:PBD
	movep	#$6bcd,x:PBD
	nop
;
; Byte 3
; bit 7		0=ok, 1=broken
; bit 6		audio auxiliary enable
; bit 5		mute
; bit 4		u bit enable
	movep	#$6b5f,x:PBD		; write four times to reg 11
	movep	#$2b5f,x:PBD
	movep	#$6b5f,x:PBD
	nop

	move	#>$ffffff,a		; set a to all 1's
	move	#>100000,b		; set loop count
	andi	#00,mr			; enable interrupts
intloop10:
	tst	a
	jge	intcont14		; go if interrupt wakes us
	sub	x0,b
	jne	intloop10		; keep waiting for interrupt
;
; Error - actel block interrupt never happened, report it
	move	#>TNAIERR,a		; report error
	move	a1,y:errnum		; tell host error number
;
; Received interrupt, check for actel nonaudio type 
intcont14:
	ori	#03,mr			; disable interrupts
	move	#>$8,x1			; check for actel nonaudio bit
	and	x1,a1
	jeq	intcont15		; ok, continue
;
; Error - actel nonaudio interrupt bit missing, report it
	move	#>TNAIBERR,a		; report error
	move	a1,y:errnum		; tell host error number
	jmp	end_irq
;
; check end of transmit loop	
	move	y:intlpcnt2,a
	move	a1,y:(r2)+		; put count in buffer
	sub	x0,a
	jne	intloop11
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; 
; Test Actel receive block interrupt 0,1 (block & nonaudio data)
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
intcont15:
	move	#>8,a			; loop count
intloop12:
	move	a1,y:intlpcnt2
	movep	#$670e,x:PBD		; Enable receive block interrupts
	movep	#$270e,x:PBD
	movep	#$670e,x:PBD
	movep	#$681f,x:PBD		; Clear Actel interrupts
	movep	#$281f,x:PBD
	movep	#$681f,x:PBD

	move	#>$beef,a			; clear interrupt stuff
	move	a,y:intreg
	move	a,y:intcnt

	move	#>$123456,a		; get a value (left or right)
	movep	a1,x:TX			; and send it
	movep	#$3100,x:CRB		; enable tx and rx ssi
	movep	y:txctrh,x:PBD		; go
	movep	y:txctrl,x:PBD
	movep	y:txctrh,x:PBD

	movep	#$625f,x:PBD		;Unreset the AES receiver and go
	movep	#$225f,x:PBD
	movep	#$625f,x:PBD

	move	#>$ffffff,a		; set a to all 1's
	move	#>100000,b		; set loop count
	andi	#00,mr			; enable interrupts
intloop2:
	tst	a
	jge	intcont3		; go if interrupt wakes us
	sub	x0,b
	jne	intloop2		; keep waiting for interrupt
;
; Error - actel block interrupt never happened, report it
	move	#>RBIERR,a		; report error
	move	a1,y:errnum		; tell host error number
	jmp	end_irq
;
; Received interrupt, check for actel block type 
intcont3:
	move	#>$1,x1			; check for actel block bit
	and	x1,a1
	jeq	intcont4		; ok, continue
;
; Error - actel block interrupt bit missing, report it
	move	#>RBIBERR,a		; report error
	move	a1,y:errnum		; tell host error number
	jmp	end_irq
;
; Process non-audio data (13 interrupts worth)
intcont4:
	movep	#$670d,x:PBD		; Enable receive data interrupts
	movep	#$270d,x:PBD
	movep	#$670d,x:PBD

	move	#>$0,a			; clear interrupt stuff
	move	a,y:intreg
	move	a,y:intcnt
intloop3:
	move	#>$ffffff,a		; set a to all 1's
	move	#>100000,b		; set loop count
	andi	#00,mr			; enable interrupts
intloop4:
	tst	a
	jge	intcont5		; go if interrupt wakes us
	sub	x0,b
	jne	intloop4		; keep waiting for interrupt
;
; Error - actel block interrupt never happened, report it
	move	#>RNAIERR,a		; report error
	move	a1,y:errnum		; tell host error number
	jmp	end_irq
;
; Received interrupt, check for actel nonaudio type 
intcont5:
	ori	#03,mr			; disable interrupts
	move	#>$2,x1			; check for actel nonaudio bit
	and	x1,a1
	jeq	intcont6		; ok, continue
;
; Error - actel nonaudio interrupt bit missing, report it
	move	#>RNAIBERR,a		; report error
	move	a1,y:errnum		; tell host error number
	jmp	end_irq
;
; Get and store nonaudio data
intcont6:
        movep   #$7f00,x:PBDDR		; Prepare to read nonaudio data
        movep   #$6300,x:PBD
        movep   #$4300,x:PBD
	nop
	nop
	nop
	nop
        movep   x:PBD,a1		; Read the nonaudio data
        movep   #$6300,x:PBD
        movep   #$7fff,x:PBDDR		; return to write mode
	move	a1,y:(r2)+		; store data in buffer
	move	#>13,x1
	move	y:intcnt,a
	cmp	x1,a
	jlt	intloop3
	movep	x:RX,a0			; read a sample
	move	a0,y:(r2)+		; and store
	move	#>$faced,a
	move	a1,y:(r2)+		; fill to 16
	move	y:intlpcnt2,a
	move	a1,y:(r2)+		; also put count
	sub	x0,a
	jne	intloop12
;
; check end of transmit-receive pairs
	move	y:intlpcnt,a
	sub	x0,a
	jne	intloop8

	movep	#$625f,x:PBD		;Unreset the AES receiver and go
	movep	#$225f,x:PBD
	movep	#$625f,x:PBD
	jmp	end_irq
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; 
; Interrupt Vector and Handler 
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
irqbvec:
	jsr	>IRQBHandler		; forces long interrupt

;   IRQBHandler:
;   This routine is called by an IRQB interrupt.  It figures out the source
;   of the interrupt and deals with it.
IRQBHandler:
	tst	a			; expecting interrupts??
	jge	irqberror		; no, report error
	move	y:intcnt,a		; count interrupts
	add	x0,a
	move	a1,y:intcnt
        movep   #$7f00,x:PBDDR		; Prepare to read interrupt reg
        movep   #$6800,x:PBD
        movep   #$4800,x:PBD
	nop
	nop
        movep   x:PBD,a1		; Read the interrupt reg
        movep   #$6800,x:PBD
        movep   #$7fff,x:PBDDR		; return to write mode
	movep	#$681f,x:PBD		; Clear Actel interrupts
	movep	#$281f,x:PBD
	movep	#$681f,x:PBD
	move	a1,y:intreg		; save a also (and delay before rti)
	move	a1,y:intreg		; save a also (and delay before rti)
	rti

irqberror:
	move	#>IRQERR,a		; report irq error
	move	a1,y:errnum
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
; 
; Interrupt Test finished
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
end_irq:
	ori	#03,mr			; disable interrupts
	movep	#$670f,x:PBD		; Disable all interrupts
	movep	#$270f,x:PBD
	movep	#$670f,x:PBD
	movep	#0,x:IPR		; Disable IRQB interrupts
	movep	#$681f,x:PBD		; Clear Actel interrupts
	movep	#$281f,x:PBD
	movep	#$681f,x:PBD
	jmp	dispatcher

;************************************************************************
;
; Test 3, Test 6, and Test 11
; Digital test 
;************************************************************************
digtst:
	movep	y:txctrofh,x:PBD	; set new sample rate and nogo
	movep	y:txctrofl,x:PBD
	movep	y:txctrofh,x:PBD

	movep	#$6200,x:PBD		; Reset the AES receiver and nogo
	movep	#$2200,x:PBD
	movep	#$6200,x:PBD

	move	#>1000,a		; let receiver/transmitter stop
wtloop3:
	sub	x0,a
	jne	wtloop3
;
; presend values to start transmitter
	move	y:(r3),a		; get a value (left or right)
	movep	a1,x:TX			; and send it
	movep	#$3100,x:CRB		; enable tx and rx ssi
	movep	y:txctrh,x:PBD		; go
	movep	y:txctrl,x:PBD
	movep	y:txctrh,x:PBD

	movep	#$621f,x:PBD		;Unreset the AES receiver and go
	movep	#$221f,x:PBD
	movep	#$621f,x:PBD
;
; send sin table through transmitter, collect data received
digtst1:
	jset	#7,x:SSISR,digrecv1
	jset	#6,x:SSISR,digsend1
	jmp	digtst1
;
; send a word
digsend1:
	move	y:(r3),a		; get a value (left or right)
	btst	#0,y:sindiv		; divide sin?
	jcc	digtst2			; no
	asr	a			; yes
	asr	a			; yes
	asr	a			; yes
digtst2:
	movep	a1,x:TX			; and send it
	movep	y:COUNT,a0		; buffer transmit clock
	move	a0,y:(r4)+
	bchg	#0,y:sendside		; test and change flag
	jcs	digtst1			; just sent left, don't bump
	move	y:(r3)+n3,a1		; just sent right, bump
	jmp	digtst1
;
; received a word, save it and check if at end of loop
digrecv1:
	movep	x:RX,a0
	move	a0,y:(r2)+		; store to buffer
	movep	y:COUNT,a0		; buffer receiver clock
	move	a0,y:(r5)+
	sub	x0,b			; check end condition
	jne	digtst1
	move	r4,y:txclkv		; save last tx clock pointer
	move	r5,y:rxclkv		; save last rx clock pointer
	movep	a1,x:TX			; clear for last send
	movep	a1,x:TX			; clear for last send
	rts

;************************************************************************
;
; Test 4, Test 7, and Test 8
; Analog test 
;************************************************************************
anltst:
	movep	y:lftglh,x:PBD 		;Set ADC left gain and left anl in
	movep	y:lftgll,x:PBD
	movep	y:lftglh,x:PBD
	jmp	comanl

;************************************************************************
;
; Test 5 and Test 9 and Test 10
; Microphone test 
;************************************************************************
mictst:
	movep	y:lftgmh,x:PBD		;Set ADC left gain and left mic in
	movep	y:lftgml,x:PBD
	movep	y:lftgmh,x:PBD

;
; common analog and microphone code
;
; presend value to start transmitter
comanl:
	move	y:(r3),a		; get a value (left or right)
	movep	a1,x:TX			; and send it
	movep	#$3100,x:CRB		; enable tx and rx ssi
	movep	y:txctrh,x:PBD		; go
	movep	y:txctrl,x:PBD
	movep	y:txctrh,x:PBD

	movep	y:rxctrh,x:PBD		; Enable A/D converter
	movep	y:rxctrl,x:PBD
	movep	y:rxctrh,x:PBD
	move	y:loops,b
anloop:
	move	b1,y:loopcount
	move	#>BUFSIZE,b		; count for buffer
	move	#bufsav,r2		; buffer for collecting data
;
; send sin table through transmitter, collect data received
anltst1:
	jset	#7,x:SSISR,anlrecv1
	jset	#6,x:SSISR,anlsend1
	jmp	anltst1
;
; send a word
anlsend1:
	move	y:(r3),a		; load a value (left or right)
	btst	#0,y:sindiv		; divide sin?
	jcc	anltst2			; no
	asr	a			; yes
	asr	a			; yes
	asr	a			; yes
anltst2:
	move	a1,y0			; and save
	bchg	#0,y:sendside		; test and change flag
	jcs	anltst3			; about to send left, don't bump
;
; handle right channel
	move	y:(r3)+n3,a		; about to send right, bump
	move	y:rightc,a		; test right channel value
	tst	a			; is it < 0?
	jpl	anltst4			; >= 0, go send user value
	move	y0,a			; else, send sin value
	jmp	anltst4
;
; handle left channel
anltst3:
	move	y:leftc,a		; test left channel value
	tst	a			; is it < 0?
	jpl	anltst4			; >= 0, go send user value
	move	y0,a			; else, send sin value
anltst4:
	movep	a1,x:TX			; and send data
	movep	y:COUNT,a0		; buffer transmit clock
	move	a0,y:(r4)+
	jmp	anltst1
;
; received a word, save it and check if at end of loop
anlrecv1:
	movep	x:RX,a0
	move	a0,y:(r2)+		; store to buffer
	movep	y:COUNT,a0		; buffer receiver clock
	move	a0,y:(r5)+
	sub	x0,b			; check end condition
	jne	anltst1
	move	y:loopcount,b
	sub	x0,b
	jne	anloop
	move	r4,y:txclkv		; save last tx clock pointer
	move	r5,y:rxclkv		; save last rx clock pointer
	movep	a1,x:TX			; clear for last send
	movep	a1,x:TX			; clear for last send
	rts

;************************************************************************
;
; Test 12
; Digital play -- digital in, digital out
;************************************************************************
digplay:
	movep	#$3100,x:CRB		; enable tx and rx ssi
	movep	#$6217,x:PBD		;Unreset the AES receiver and go
	movep	#$2217,x:PBD
	movep	#$6217,x:PBD
;
; preload 128 samples into rate matching buffer
	move	#bufsav,r7
	move	#$ff,m7
	do	#128,digply1
digply2:
	jclr	#7,x:SSISR,digply2
	movep	x:RX,y:(r7)+
digply1:
	move	#bufsav,r6
	move	#$ff,m6
	nop
	movep	y:(r6)+,x:TX		;Pre-stuff one sample into ssi tx.
	movep	y:txctrh,x:PBD		; go
	movep	y:txctrl,x:PBD
	movep	y:txctrh,x:PBD
digply3:
	jset	#6,x:SSISR,digplsnd
	jset	#7,x:SSISR,digplrcv
	jmp	digply3
digplsnd:
	movep	y:(r6)+,x:TX
	jmp	digply3
digplrcv:
	movep	x:RX,y:(r7)+
	jmp	digply3

;************************************************************************
;
; Test 13
; Analog play -- analog in left channel, analog in right
;************************************************************************
anlplay:
	movep	y:lftglh,x:PBD		;Set ADC left gain and left anl in
	movep	y:lftgll,x:PBD
	movep	y:lftglh,x:PBD
	jmp	comaply

;************************************************************************
;
; Test 14
; Microphone play -- microphone in left channel, analog in right
;************************************************************************
micplay:
	movep	y:lftgmh,x:PBD		;Set ADC left gain and left mic in
	movep	y:lftgml,x:PBD
	movep	y:lftgmh,x:PBD

;
; common analog and microphone play
;
; Preload 128 samples into the rate matching buffer.
comaply:
	movep	#$3100,x:CRB		; enable tx and rx ssi
	movep	y:rxctrh,x:PBD		;Enable the A/D Converter
	movep	y:rxctrl,x:PBD
	movep	y:rxctrh,x:PBD
	move	#bufsav,r7
	move	#255,m7
	do	#128,anlply3
anlply2:
	jclr	#7,x:SSISR,anlply2
	movep	x:RX,y:(r7)+
anlply3:
	move	#bufsav,r6
	move	#255,m6
	nop
	movep	y:(r6)+,x:TX		;Pre-stuff one sample into ssi tx.
	movep	y:txctrh,x:PBD		; go
	movep	y:txctrl,x:PBD
	movep	y:txctrh,x:PBD
anlply4:
	jset	#6,x:SSISR,anplsend
	jset	#7,x:SSISR,anplrecv
	jmp	anlply4
anplsend:
	movep	y:(r6)+,x:TX
	jmp	anlply4
anplrecv:
	movep	x:RX,y:(r7)+
	jmp	anlply4

;************************************************************************
;
; Test 15
; Sinplay -- Loop forever sending a specified sin wave
;************************************************************************
sinplay:
;
; presend value to start transmitter
	move	y:(r3),a		; get a value (left or right)
	movep	a1,x:TX			; and send it
	movep	#$3100,x:CRB		; enable tx and rx ssi
	movep	y:txctrh,x:PBD		; go
	movep	y:txctrl,x:PBD
	movep	y:txctrh,x:PBD
;
; continue to send sin table through transmitter
sinply1:
	jclr	#6,x:SSISR,sinply1
;
; send a word
	move	y:(r3),a		; get a value (left or right)
	btst	#0,y:sindiv		; divide sin?
	jcc	sinply2			; no
	asr	a			; yes
	asr	a			; yes
	asr	a			; yes
sinply2:
	movep	a1,x:TX			; and send it
	bchg	#0,y:sendside		; test and change flag
	jcs	sinply1			; just sent left, don't bump
	move	y:(r3)+n3,a1		; just sent right, bump
	jmp	sinply1

;************************************************************************
;
; End of tests
;************************************************************************

	org	y:$8010
;
; DSP addresses that will be read by the host software
;
revlev:	ds	1			; revision level
txclkv:	ds	1			; end tx clk pointer
rxclkv:	ds	1			; end rx clk pointer
cp10ms:	ds	1			; CountsPerTenMilliseconds
errnum:	ds	1			; returned error number
	ds	11			; pad until $8020
;
; DSP addresses that will be written by the host software
;
hshake:	ds	1			; hand shake location ($8020)
frqinc:	ds	1			; frequency increment
ingain:	ds	1			; input gain
volume:	ds	1			; output gain
sindiv:	ds	1			; sin divisor
txsrf:	ds	1			; transmit sample rate frequency
rxsrf:	ds	1			; receiver sample rate frequency
loops:	ds	1			; number of internal loops
leftc:	ds	1			; left channel value
rightc:	ds	1			; right channel value

	org	y:$c000
dispatchtab:
	dc	Test0
	dc	Test1
	dc	Test2
	dc	Test3
	dc	Test4
	dc	Test5
	dc	Test6
	dc	Test7
	dc	Test8
	dc	Test9
	dc	Test10
	dc	Test11
	dc	Test12
	dc	Test13
	dc	Test14
	dc	Test15
command:	ds	1
sendside:	ds	1		; flag for send left or right
lftgmh:		ds	1		; left gain mic hi strobe
lftgml:		ds	1		; left gain mic lo strobe
lftglh:		ds	1		; left gain line hi strobe
lftgll:		ds	1		; left gain line lo strobe
rtgh:		ds	1		; right gain hi strobe
rtgl:		ds	1		; right gain lo strobe
txctrl:		ds	1		; transmit frequency control lo
txctrh:		ds	1		; transmit frequency control hi
txctrofl:	ds	1		; transmit frequency control lo off
txctrofh:	ds	1		; transmit frequency control hi off
rxctrl:		ds	1		; receiver frequency control lo
rxctrh:		ds	1		; receiver frequency control hi
loopcount:	ds	1		; loop count for analog
intreg:		ds	1		; actel interrupt status register
intcnt:		ds	1		; interrupt count
intlpcnt:	ds	1		; interrupt loop count
intlpcnt2:	ds	1		; interrupt loop count 2

	org	y:$c200
sintab:		ds	512		; sin table for comparison

	org	y:$c400
txclk:		ds	512		; transmit sample rate clock

	org	y:$c600
rxclk:		ds	512		; receiver sample rate clock

	org	y:$d000
bufsav:		ds	BUFSIZE		; audio data for mips (just raw)
	endsec

