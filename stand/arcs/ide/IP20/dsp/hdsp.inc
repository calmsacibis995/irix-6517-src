;
;	hdsp.inc -- header file for diagnostic assembler routines
;

; HPC1 registers

DMAWDCNT	equ	y:$ffe0
GIOADDL		equ	y:$ffe1
GIOADDM		equ	y:$ffe2
PBUSADD		equ	y:$ffe3
DMACTRL		equ	y:$ffe4
COUNT  		equ	y:$ffe5
HANDTX		equ	y:$ffe6
HANDRX		equ	y:$ffe7
INTSTAT		equ	y:$ffe8
INTMASK		equ	y:$ffe9
INTCONF		equ	y:$ffea
INTPOL		equ	y:$ffeb
MISCCSR		equ	y:$ffec
BURSTCTL	equ	y:$ffed

; DSP56001 registers

SCR		equ	x:$fff0		; SCI interface control register
SCCR		equ	x:$fff2		; SCI control register
BCR		equ	x:$fffe		; port a - bus control register
IPR		equ	x:$ffff		; interrupt priority register
PBC         	equ     x:$ffe0         ; port b control register
PCC         	equ     x:$ffe1         ; ssi port c control register
PBDDR       	equ     x:$ffe2         ; port b data direction register
PBD         	equ     x:$ffe4         ; port b data register
CRA         	equ     x:$ffec         ; ssi control register a
CRB         	equ     x:$ffed         ; ssi control register b
SSISR       	equ     x:$ffee         ; ssi status register
TX          	equ     x:$ffef         ; ssi transmit register
RX          	equ     x:$ffef         ; ssi receive register
HEADPHONE_MDAC_L equ        y:$fffc
HEADPHONE_MDAC_R equ        y:$fffd

; DSP56001 Interrupt Sources

IRQA		equ	p:$0008
IRQB		equ	p:$000a

; Important constants

THE_IPR			equ	$b001
IRQB_INT		equ	$8
MaxCalloutListEntries	equ	20
