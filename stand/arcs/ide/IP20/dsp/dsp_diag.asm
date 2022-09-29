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
DSP_HPC_HANDTX	equ	y:$ffe6		; handshake transmit
DSP_HPC_HANDRX	equ	y:$ffe7		; handshake receive
DSP_HPC_INTSTAT	equ	y:$ffe8		; interrupt status
DSP_HPC_INTMASK	equ	y:$ffe9		; interrupt mask
DSP_HPC_INTCONF	equ	y:$ffea		; interrupt configuration
DSP_HPC_INTPOL	equ	y:$ffeb		; interrupt polarity
DSP_HPC_MISCSR	equ	y:$ffec		; miscellaneous control and status
DSP_HPC_BURSTCTL equ	y:$ffed		; dma burst control register

; DUART
DUART0A_CNTRL	equ	y:$fff2
DUART1A_CNTRL	equ	y:$fff6
DUART2A_CNTRL	equ	y:$fffa
DUART3A_CNTRL	equ	y:$fffe

; real time clock (RTC)
RTC_BASE	equ	y:$ffc0
RTC_PAGE	equ	7

; bit definitions in INSTAT, INTMASK, INTCONF, INTPOL
DMA_DONE	equ	0
HANDTX		equ	5
HANDRX		equ	6

; test status return to the MIPS
BAD_DUART0		equ	0
BAD_DUART1		equ	1
BAD_DUART2		equ	2
BAD_DUART3		equ	3
BAD_RTC			equ	4
BAD_HPC			equ	5
BAD_DSP_WRITE_MIPS	equ	6
BAD_DSP_READ_MIPS	equ	7
BAD_DMA_DATA		equ	8
BAD_SRAM_MAP		equ	9

; miscellaneous constant
WAIT_COUNT	equ	1000
INTERRUPT_DELAY	equ	10000



	org	p:$e000

; here is where all begin
bang:
	movep	#>$4,x:<<DSP_BCR		; 4 wait states per I/O access
	movep	#>$0,y:<<DSP_HPC_DMACTRL	;
	movep	#>$0,y:<<DSP_HPC_INTSTAT	;
	movep	#>$0,y:<<DSP_HPC_INTMASK	;
	movep	#>$61,y:<<DSP_HPC_INTCONF	;
	movep	#>$0,y:<<DSP_HPC_INTPOL		;
	movep	#>$48,y:<<DSP_HPC_MISCSR	; 32K SRAM, Sign Extend
	movep	#$4100,y:<<DSP_HPC_BURSTCTL	; 

	movep	#>$3,x:<<DSP_IPR		; IRQA at priority level 2
	movep	#>(1<<HANDTX)|(1<<HANDRX),y:<<DSP_HPC_INTMASK	;
	movec	#>$0,sr				; enable interrupts in general

; set up the IRQA interrupt vector
	movem	p:IRQAvec,a1			;
	movem	a1,p:<$8			; IRQA interrupt vector
	movem	p:IRQAvec+1,a1			;
	movem	a1,p:<$9			;

	move	#<$0,r6				; HANDTX interrupt counter
	move	#>$ffff,m6			;

; Tell the MIPS that the DSP is running.

	move	#$cab,a1
	move	a1,y:$8000

	jsr	DSP_diag			; do diagnostics

; wait for the MIPS to interrupt
idle:
	nop					;
	jmp	idle				;

IRQAvec:
	jsr	IRQAHandler			;



;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

; when receives a HANDTX interrupt, increment the TX interrupt counter and
; then clear the interrupt
IRQAHandler:
; note - it's important to check for the handtx interrupt before the
; handrx interrupt because if they both are set, we must increment r6 before
; we send r6 back to the mips.
	jset	#HANDTX,y:<<DSP_HPC_INTSTAT,_HANDTX_handler	;
	jset	#HANDRX,y:<<DSP_HPC_INTSTAT,_HANDRX_handler	;

_HANDTX_handler:
	move	(r6)+				; increment counter
;	bclr	#HANDTX,y:<<DSP_HPC_INTSTAT	; clear HANDTX interrupt
	movep	#~(1<<HANDTX),y:<<DSP_HPC_INTSTAT
	rti					;

; when receives a HANDRX interrupt, read the HANDRX register to
; generate an HANDRX interrupt to the MIPS, clear the HANDRX interrupt
; and write the complement of what we read from the HANDRX register
; into the HANDTX register.  if 0 is what we read from the HANDRX
; register, send the HANDTX interrupt count instead
_HANDRX_handler:
   move a0,y:a0x
   move a1,y:a1x
   move a2,y:a2x
   move b0,y:b0x
   move b1,y:b1x
   move b2,y:b2x
   move x0,y:x0x
	clr	a				;
	movep	y:<<DSP_HPC_HANDRX,a1		; acknowledge
;	bclr	#HANDRX,y:<<DSP_HPC_INTSTAT	; clear the interrupt
	movep	#~(1<<HANDRX),y:<<DSP_HPC_INTSTAT

	move	#>$ffff,x0			; register mask
	and	x0,a				;
	tst	a				;
	jne	_handshake_test			;

	move	r6,a1				; send HANDTX interrupt count
	jmp	_send				;

_handshake_test:
	not	a				;

; give the MIPS enough time, 1 msec
_send:
	move	#>INTERRUPT_DELAY,b		;
	do	b,_delay			;
	nop					;
_delay:

	movep	a1,y:<<DSP_HPC_HANDTX		;
   move y:a0x,a0
   move y:a1x,a1
   move y:a2x,a2
   move y:b0x,b0
   move y:b1x,b1
   move y:b2x,b2
   move y:x0x,x0
	rti					;




;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

Xjclr	MACRO	bit,reg,target
	move	reg,y:Xtemp
	jclr	bit,y:Xtemp,target
	ENDM
Xbset	MACRO	bit,reg
	move	reg,y:Xtemp
	bset	bit,y:Xtemp
	move	y:Xtemp,reg
	ENDM
Xbclr	MACRO	bit,reg
	move	reg,y:Xtemp
	bclr	bit,y:Xtemp
	move	y:Xtemp,reg
	ENDM


DSP_diag:
	move	#<0,r7				; assume everything is OK

	jmp	around
_DUART0:
	move	#>DUART0A_CNTRL,r0		;
	jsr	DUART_test			;
	Xjclr	#0,a1,TEST_DUART1		;
	Xbset	#BAD_DUART0,r7			;

TEST_DUART1:
	move	#>DUART1A_CNTRL,r0		;
	jsr	DUART_test			;
	Xjclr	#0,a1,TEST_DUART2		;
	Xbset	#BAD_DUART1,r7			;

TEST_DUART2:
	move	#>DUART2A_CNTRL,r0		;
	jsr	DUART_test			;
	Xjclr	#0,a1,RTC			;
	Xbset	#BAD_DUART2,r7			;
around:

RTC:
	jsr	RTC_test			;

_HPC:
	jsr	HPC_test			;

_DMA:
	jsr	DMA_test			;

_SRAM_MAP:
	jsr	SRAM_map_test			;

; pass test result back to the MIPS
_diag_done:
	movep	r7,y:<<DSP_HPC_HANDTX		;
	rts					;



;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;



; test the connection between the DUART and the DSP
; write different values into DUART write registers 12 and 13, then read them
; back and verify.  clear accA if everything is OK, otherwise, set accA to 1.
; address of channel A control register must be passed through r0
DUART_test:
; write the first value, $55
	move	#<12,n0				;
	move	n0,y:(r0)			;
	nop					;
	move	#>$55,y0			;
	move	y0,y:(r0)			;
	nop

; write the second value, $aa
	move	#<13,n0				;
	move	n0,y:(r0)			;
	nop					;
	move	#>$aa,y1			;
	move	y1,y:(r0)			;
	nop

	move	#>$ff,x0			; data mask

; read back the first value, should be $55
	move	#<12,n0				;
	move	n0,y:(r0)			;
	clr	a				;
	nop					;
	move	y:(r0),a1			;

; verify
	and	x0,a				; 
	cmp	y0,a				;
	jeq	_next_register			;

	move	#>$1,a				;
	rts					;

_next_register:
; read back the second value, should be $aa
	move	#<13,n0				;
	move	n0,y:(r0)			;
	clr	a				;
	nop					;
	move	y:(r0),a1			;

; verify
	and	x0,a				;
	cmp	y1,a				;
	jeq	_DUART_test_done		;

	move	#>$1,a				;
	rts					;

_DUART_test_done:
	clr	a				;
	rts



;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;



; test the connection between the RTC and the DSP
; write different values into different RAM locations in the RTC chip,
; then read them back and verify
RTC_test:
	move	#>RTC_BASE,r0			;
	move	#>$ffff,m0			;
	move	#>$ff,x0			; data mask
	bset	#RTC_PAGE,y:(r0)+		; select RAM page on RTC chip

; write value to RTC RAM, $55 and $aa
	move	#>$55,y0			;
	move	y0,y:(r0)+			;
	nop					;
	move	#>$aa,y1			;
	move	y1,y:(r0)			;

	move	#>RTC_BASE+1,r0			; first location in RAM
	clr	a				;

; read back and verify
	move	y:(r0)+,a1			;
	and	x0,a				;
	cmp	y0,a				;
	jeq	_next_address			;

	Xbset	#BAD_RTC,r7			;
	rts

_next_address:
	move	y:(r0),a1			;
	and	x0,a				;
	cmp	y1,a				;
	jeq	_RTC_test_done			;

	Xbset	#BAD_RTC,r7			;
	rts					;

_RTC_test_done:
	Xbclr	#BAD_RTC,r7			;
	rts					;




;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;



; test the connection between the HPC and the DSP
; write different values into different locations in the HPC address space,
; then read them back and verify
HPC_test:
	move	#>DSP_HPC_GIOADDL,r0		;
	move	#>$ffff,m0			;
	move	m0,x0				; data mask

; write values into HPC address space
	move	#>$5555,y0			;
	move	y0,y:(r0)+			;
	move	#>$aaaa,y1			;
	move	y1,y:(r0)			;

	move	#>DSP_HPC_GIOADDL,r0		;
	clr	a				;

; read them back and verify
	move	y:(r0)+,a1			;
	and	x0,a				;
	cmp	y0,a				;
	jeq	_next_address			;

	Xbset	#BAD_HPC,r7			;
	rts

_next_address:
	move	y:(r0),a1			;
	and	x0,a				;
	cmp	y1,a				;
	jeq	_HPC_test_done			;

	Xbset	#BAD_HPC,r7			;
	rts					;

_HPC_test_done:
	Xbclr	#BAD_HPC,r7			;
	rts					;



;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;



; test the DMA connection between the DSP and the MIPS
; first DMA from the DSP to the MIPS (0x8400000) and then DMA back from
; the MIPS to the DSP.  verify the data read back with the original data
DMA_test:
	move	#>DSP_HPC_BYTECNT,r0		;
	move	#>$ffff,m0			;
	move	#>$1,x0				; counter decrement

	move	#>DMA_COUNT,a			;
	move	a1,y:(r0)+			; DMA count

	clr	a				;
	move	a1,y:(r0)+			; GIO-bus address, LSB
	move	#>$840,a			;
	move	a1,y:(r0)+			; GIO-bus address, MSB

	move	#>pattern,a			;
	move	a1,y:(r0)+			; PBUS address

	move	#>$3,a				; DMA to GIO bus
	move	a1,y:(r0)			; go

	move	#>DSP_HPC_INTSTAT,r0		;
	move	#>WAIT_COUNT,a			;

	Xbset	#BAD_DSP_WRITE_MIPS,r7		; assume DMA failure

; wait for DMA to finish
_to_GIO_wait:
	jset	#DMA_DONE,y:(r0),_to_GIO_done	;
	sub	x0,a				;
	jne	_to_GIO_wait			;
	rts					;

_to_GIO_done:
	Xbclr	#BAD_DSP_WRITE_MIPS,r7		; DMA OK

	move	#>DSP_HPC_BYTECNT,r0		;

	move	#>DMA_COUNT,a			;
	move	a1,y:(r0)+			; DMA count

	clr	a				;
	move	a1,y:(r0)+			; GIO-bus address, LSB
	move	#>$840,a			;
	move	a1,y:(r0)+			; GIO-bus address, MSB

	move	#>read_back,a			;
	move	a1,y:(r0)+			; PBUS address

	movep	#~(1<<DMA_DONE),y:<<DSP_HPC_INTSTAT ;clear dma done in intstat
	move	#>$1,a				; DMA from GIO bus
	move	a1,y:(r0)			; go

	move	#>DSP_HPC_INTSTAT,r0		;

	move	#>WAIT_COUNT,a			;

	Xbset	#BAD_DSP_READ_MIPS,r7		; assume DMA failure

; wait for DMA to finish
_from_GIO_wait:
	jset	#DMA_DONE,y:(r0),_from_GIO_done	;
	sub	x0,a				;
	jne	_from_GIO_wait			;
	rts					;

_from_GIO_done:
	Xbclr	#BAD_DSP_READ_MIPS,r7		; DMA OK

	move	#>pattern,r0			; original data
	move	#>read_back,r1			; data read
	move	m0,m1				;
	clr	a				;

	move	#>DMA_COUNT,b			;

; verify the read data against the original data
_verify:
	move	y:(r0)+,a			;
	move	y:(r1)+,x1			;
	cmp	x1,a				;
	jne	_DMA_bad			;
	sub	x0,b				;
	jne	_verify				;

	Xbclr	#BAD_DMA_DATA,r7		; data check out OK
	rts					;

_DMA_bad:
	Xbset	#BAD_DMA_DATA,r7		;
	rts					;



;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;



; verify the mapping of X:, Y:, and P: space as documented in the PBUS spec
SRAM_map_test:
; P: space is mapped into X: space directly
	clr	a				;
	move	#>$1,x0				; decrement count

	move	#>$e000,r0			; P: space
	move	#>$ffff,m0			;
	move	#>$1000,b			; test 4K locations

_verify_PX_0:
	movem	p:(r0),a			;
	move	x:(r0)+,x1			;
	cmp	x1,a				;
	jne	_bad_map			;
	sub	x0,b				;
	jne	_verify_PX_0			;

; exclusive-or A14 with 1 to get from P: space to Y: space
	move	#>$e000,r0			; P: space
	move	#>$a000,r1			; Y: space
	move	m0,m1				;
	move	#>$1000,b			; test 4K locations

_verify_PY_0:
	movem	p:(r0)+,a			;
	move	y:(r1)+,x1			;
	cmp	x1,a				;
	jne	_bad_map			;
	sub	x0,b				;
	jne	_verify_PY_0			;

	move	#>$8000,r0			; P: space
	move	#>$1000,b			; test 4K locations

_verify_PX_1:
	movem	p:(r0),a			;
	move	x:(r0)+,x1			;
	cmp	x1,a				;
	jne	_bad_map			;
	sub	x0,b				;
	jne	_verify_PX_1			;

; exclusive-or A14 with 1 to get from P: space to Y: space
	move	#>$8000,r0			; P: space
	move	#>$c000,r1			; Y: space
	move	#>$1000,b			; test 4K locations

_verify_PY_1:
	movem	p:(r0)+,a			;
	move	y:(r1)+,x1			;
	cmp	x1,a				;
	jne	_bad_map			;
	sub	x0,b				;
	jne	_verify_PY_1			;

	Xbclr	#BAD_SRAM_MAP,r7		;
	rts					;

_bad_map:

	Xbset	#BAD_SRAM_MAP,r7		;
	rts



;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;



	org	y:$c000

DMA_COUNT	equ	48
pattern:
	dc	$000001,$000002,$000004,$000008
	dc	$000010,$000020,$000040,$000080
	dc	$000100,$000200,$000400,$000800
	dc	$001000,$002000,$004000,$008000
	dc	$010000,$020000,$040000,$080000
	dc	$100000,$200000,$400000,$800000
	dc	~$000001,~$000002,~$000004,~$000008
	dc	~$000010,~$000020,~$000040,~$000080
	dc	~$000100,~$000200,~$000400,~$000800
	dc	~$001000,~$002000,~$004000,~$008000
	dc	~$010000,~$020000,~$040000,~$080000
	dc	~$100000,~$200000,~$400000,~$800000
read_back:
	ds	DMA_COUNT
a0x	ds	1
a1x	ds	1
a2x	ds	1
b0x	ds	1
b1x	ds	1
b2x	ds	1
x0x	ds	1
	endsec
