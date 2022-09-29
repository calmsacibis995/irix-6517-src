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
;1st SGI version=20.001
vers	equ	20006

;resolution of "clock"
mhz	equ	8		;CPU clock frequency
clkres	equ	10		;board timer resolution in usec


;scratch pad RAM locations for each port
;
brkclk	equ	06h	;stop BREAK clock
;
prvst	equ	08h	;previous status
txon	equ	09h	;RR0_TXE if output needed
brkbit	equ	0ah	;80h after RR0_BRK
;
wend	equ	10h	;write end offset
wptr	equ	12h	;current write offset
wend2	equ	14h	;future write end
wptr2	equ	16h	;future write start
;
padvlen	equ	26h	;reset this many scratch pad bytes when changing BAUD
;
wr3	equ	26h	;write register 3 for scc - byte
wr4	equ	27h	;write register 4 for scc - byte
wr5	equ	28h	;write register 5 for scc - byte
wr10	equ	29h	;write register 10 for scc - byte
wr12	equ	2ah	;write register 12 for scc - byte
wr13	equ	2bh	;write register 13 for scc - byte
wr14	equ	2ch	;write register 14 for scc - byte

padlen	equ	40h	;bytes/port

;the start of scratch pad SRAM for each port
po0	equ	130h		;port 0
po1	equ	po0+padlen*1
po2	equ	po0+padlen*2
po3	equ	po0+padlen*3
po4	equ	po0+padlen*4
po5	equ	po0+padlen*5
po6	equ	po0+padlen*6
po7	equ	po0+padlen*7



intvec	equ	320h	;interrupt vector - word
intlvl	equ	322h	;interrupt level
intbits	equ	324h	;interrupt input, output & status request bits

;bits in intbits
intbit_cmd equ	80h
intbit_dcd equ	04h
intbit_in  equ	02h
intbit_out equ	01h

idlyval	equ	326h	;input delay
idlyclk	equ	328h	;clock at last interrupt
clk	equ	32ah	;board timer


;**********************************************************************
;Offsets into each dual-port RAM buffer
;
inlen	equ	800h		;bytes/input buffer
outlen	equ	3056		;bytes/output buffer
dplen	equ	2000h		;bytes/port

ib	equ	0h		;input buffer 
ob0	equ	ib+inlen	;output buffer 0
ob1	equ	ob0+outlen	;output buffer 1
;
pad1	equ	1fe0h		;clear from here to end at start up
cmdcd	equ	1fe1h		;command code - byte
cmdd	equ	1fe2h		;command data - word
intrx	equ	1fe4h		;end-of-command interrupt - port 2 only - byte
cmds	equ	1fe5h		;command status - byte
sfs	equ	1fe7h		;status from scc byte
ifp	equ	1fe8h		;input filling pointer - word
;	equ	1feah
;	equ	1fech
isfx	equ	1fefh		;interrupt source flag - port 2 only - byte
opb	equ	1ff1h		;output busy - byte
;	equ	1ff3h
;	equ	1ff5h
rev	equ	1ffeh		;firmware revision number - word
;	equ	1ff7h

isf	equ	isfx+dplen*2
intr	equ	intrx+dplen*2


;**********************************************************************
;hardware addresses
;
rdstat  equ	0020h	;8088 status port
rdcmd	equ	0021h	;new cmd port
rdring	equ	0023h	;ring indicator port

;SCC RR0 bits
RR0_RX	equ	01h	;RX character available
RR0_TXE	equ	04h	;TX Buffer Empty
RR0_DSR	equ	08h	;SCC calls it DCD, but we use it as DSR
RR0_DCD	equ	10h	;SCC calls it RI, but we use it as DCD
RR0_CTS	equ	20h
RR0_BRK	equ	80h	;BREAK detected

;SCC RR1 bits
RR1_AS	equ	01h	;all sent
RR1_PE	equ	10h	;parity error
RR1_OE	equ	20h	;overrun
RR1_FE	equ	40h	;framing error

WR0_ER	equ	30h	;error reset
WR0_RE	equ	10h	;reset external status interrupts

dpa0	equ	3000h	;unlocked - base adr dual buffer for port 0

do0	equ	0	;offset of port 0 dual port buffer
do1	equ	do0+dplen*1
do2	equ	do0+dplen*2
do3	equ	do0+dplen*3
do4	equ	do0+dplen*4
do5	equ	do0+dplen*5
do6	equ	do0+dplen*6
do7	equ	do0+dplen*7

scc0	equ	40h	;base addr port0
scc1	equ	48h	;base addr port1
scc2	equ	50h	;base addr port2
scc3	equ	58h	;base addr port3
scc4	equ	60h	;base addr port4
scc5	equ	68h	;base addr port5
scc6	equ	70h	;base addr port6
scc7	equ 	78h	;base addr port7


;write something to all of the SCCs
wwrall	macro	v
	local	lop
	mov	al,v
	mov	dx,scc0
lop:	out	dx,al
	add	dx,scc2-scc0
	cmp	dx,scc6
	jle	lop
	endm


;clear the SCCs
;	reset all channels by writing 0c0h,80h,scc0 to wr9 of each scc
;	Wr9 is shared by both channels of each SCC
;   changes al and dx
initscc	macro
	local	dumread
	mov	dx,scc0
dumread:in	al,dx   	;read reg 0 of all channels
	nop
	mov	al,1		;point to reg 1
	out	dx,al
	nop
	in	al,dx		;do dummy read of reg 1
	nop
	add	dx,8
	cmp	dx,scc7
	jle	dumread

	wwrall	09h
	wwrall	0c0h		

	wwrall	09h
	wwrall	080h		;reset channel a

	wwrall	09h
	wwrall	040h		;reset channel b
	endm
