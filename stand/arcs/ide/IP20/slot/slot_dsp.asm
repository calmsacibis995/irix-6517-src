; DSP diagnostics microcode

	section	DSP_diag
	opt	mex

Xtemp:	equ	y:0


; DSP address
DSP_BCR		equ	x:$fffe		; bus control register
DSP_IPR		equ	x:$ffff		; interrupt priority register

; DSP-HPC addresses
DSP_HPC_BYTECNT	equ	y:$ffe0		; DMA transfer size
DSP_HPC_GIOADDL	equ	y:$ffe1		; GIO-bus address, LSB
DSP_HPC_GIOADDM	equ	y:$ffe2		; GIO-bus address, MSB
DSP_HPC_PBUSADD	equ	y:$ffe3		; Pbus address
DSP_HPC_DMACTRL	equ	y:$ffe4		; DMA control
DSP_HPC_INTSTAT	equ	y:$ffe8		; interrupt status
DSP_HPC_HANDTX	equ	y:$ffe6		; handshake transmit
DSP_HPC_INTMASK	equ	y:$ffe9		; interrupt mask
DSP_HPC_INTCONF	equ	y:$ffea		; interrupt configuration
DSP_HPC_INTPOL	equ	y:$ffeb		; interrupt polarity
DSP_HPC_MISCSR	equ	y:$ffec		; miscellaneous control and status

; real time clock (RTC)

; bit definitions in INSTAT, INTMASK, INTCONF, INTPOL
DUART3 		equ	4			; pbus 1 int
HANDTX		equ	5
HANDRX		equ	6
MODEM 		equ	7			; pbus 0 int

; test status return to the MIPS

; miscellaneous constant
WAIT_COUNT	equ	10000

	org	p:$e000

; here is where all begin
bang:
	movep	#>$4,x:<<DSP_BCR		; 4 wait states per I/O access
;	movep	#>$0,y:<<DSP_HPC_DMACTRL	;
	movep	#>$0,y:<<DSP_HPC_INTSTAT	;
	movep	#>$0,y:<<DSP_HPC_INTMASK	;
	movep	#>$61,y:<<DSP_HPC_INTCONF	; edge sensing for DMA, HANDTX,HANDRX,
										; level sensing for PINT0 PINT1
	movep	#>$0,y:<<DSP_HPC_INTPOL		;
	movep	#>$48,y:<<DSP_HPC_MISCSR	; 32K SRAM, Sign Extend

	movep	#>$3,x:<<DSP_IPR		; IRQA at priority level 2
;	movep	#>(1<<DUART3)|(1<<MODEM)|(1<<HANDTX)|(1<<HANDRX),y:<<DSP_HPC_INTMASK
;	movep	#>(1<<DUART3)|(1<<MODEM),y:<<DSP_HPC_INTMASK

	movep	#>$0,y:<<DSP_HPC_INTMASK	;
; Tell the MIPS that the DSP is running.
	move	#$111,a1
	move	a1,y:$8000
	nop
	nop
	nop

	movep	#>(1<<DUART3),y:<<DSP_HPC_INTMASK

	movec	#>$0,sr				; enable interrupts in general

; set up the IRQA interrupt vector
	movem	p:IRQAvec,a1			;
	movem	a1,p:<$8			; IRQA interrupt vector
	movem	p:IRQAvec+1,a1			;
	movem	a1,p:<$9			;


; wait for the MIPS to interrupt
idle:
	nop					;
	jmp	idle				;

IRQAvec:
	jsr	IRQAHandler			;



;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

IRQAHandler:

;	jset	#MODEM,y:<<DSP_HPC_INTSTAT,MODEM_handler
;	jset	#HANDRX,y:<<DSP_HPC_INTSTAT,HANDRX_handler	;
;	jset	#HANDTX,y:<<DSP_HPC_INTSTAT,HANDTX_handler	;
	jset	#DUART3,y:<<DSP_HPC_INTSTAT,DUART3_handler	;

	rti

MODEM_handler:

idle2:
	move	#$1234,a1
	move	a1,y:$8000
	movep	#~(1<<MODEM),y:<<DSP_HPC_INTSTAT ; clear the interrupt
;	jmp		idle2

	rti		; ***************


DUART3_handler:
idle1:
	move	#$4321,a1
	move	a1,y:$8000

	movep	#~(1<<DUART3),y:<<DSP_HPC_INTSTAT ; clear the interrupt
;	jmp		idle1

	rti		; ***************


HANDTX_handler:
idle4:
	move	#$222,a1
	move	a1,y:$8000

;	jmp		idle4
	movep	#~(1<<HANDTX),y:<<DSP_HPC_INTSTAT
	rti					;

HANDRX_handler:

idle3:
	move	#$555,a1
	move	a1,y:$8000

;	jmp		idle3

	rti 	; ***************


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

	org	y:$c000

what_int dc	0

	endsec
