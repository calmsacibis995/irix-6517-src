;hardware definitions for E-Plex 8-port Ethernet card
;
; Copyright 1994 Silicon Graphics, Inc.  All rights reserved.
;	"$Revision: 1.12 $"


	.equ	EP_PORTS,	8	;number of Ethernet MAC chips

;approximate speed of the board
	.equ	MHZ, 25
	.equ	MHZ_BT,4		;binary log of MHZ

;board address space
	.equ	EPROM,	    0x000000	;start of EEPROM
	.equ	EPROM_SIZE, 128*1024

	.equ	LINKS,	    0x020000	;LINK bits
	.equ	LINK_REV,	1	;   in board revision 1 and later

	.equ	DMA_WC,	    0x040000	;DMA word count and direction
	.equ    DMA_ADDRHI, 0x040004	;host DMA address, bits 63:32
	.equ	DMA_ADDRLO, 0x040008	;host DMA address, bits 31:0
	.equ    LEDS,	    0x040010	;LEDs
	.equ	INTC,	    0x040014	;interrupt control
	.equ	SONIC_RESET,0x040018	;reset SONIC
	DEFBT	SONIC_RESET0,	0	;bit 0 resets SONIC #0
	.equ    GSOR,	    0x04001c	;GIO slave operations register

	.equ	SONIC0,	    0x050000	;SONIC Ethernet MACs
	.equ	SONIC1,	    0x050100
	.equ	SONIC2,	    0x050200
	.equ	SONIC3,	    0x050300
	.equ	SONIC4,	    0x050400
	.equ	SONIC5,	    0x050500
	.equ	SONIC6,	    0x050600
	.equ	SONIC7,	    0x050700
	DEFBT	SONIC_INC, 8
	CK	SONIC_INC == (SONIC1-SONIC0)

	.equ    GIORAM,	    0x080000	;start of DMA and 29K program SRAM
	.equ	GIORAM_SIZE,512*1024

	.equ	SONICRAM03, 0x100000	;2 banks of SRAM for SONICs
	.equ	SONICRAM47, 0x180000
	.equ	SONICRAM03_SIZE,512*1024
	DEFBT	SONICRAM_SIZE, 17	;share for each SONIC
	CK	SONICRAM_SIZE == SONICRAM03_SIZE/4

	.equ	DMA_BC,	    0x200000	;DMA address and byte count


;bits in the DMA_WC register
	DEFBT	DMA_WC_H2B,	31	;1=host-to-board
	.equ	DMA_WC_MASK,	0xfff
	.equ	DMA_WC_H2B_MAX,	4088
	.equ	DMA_WC_B2H_MAX,	4092

	DEFBT	DMA_ALIGN,	12	;must align board buffers to this
	DEFBT	DMA_ROUND,	2	;and this

	DEFBT	LOADM_ALIGN,	12	;size of burst counter


;bits in the LED register
	DEFBT	LED_YELLOW,	0	;1=yellow LED on
	DEFBT	LED_GREEN0,	1	;1=green LED0 on
	DEFBT	LED_GREEN1,	2	;1=green LED1 on
	DEFBT	LED_GREEN2,	3	;1=green LED2 on

;bits in the GSOR
	DEFBT	GSOR_SIR0,	0	;1=SONIC int 0 pending
	DEFBT	GSOR_SIR1,	1	;	     1
	DEFBT	GSOR_SIR2,	2	;	     2
	DEFBT	GSOR_SIR3,	3	;	     3
	DEFBT	GSOR_SIR4,	4	;	     4
	DEFBT	GSOR_SIR5,	5	;	     5
	DEFBT	GSOR_SIR6,	6	;	     6
	DEFBT	GSOR_SIR7,	7	;	     7
	.equ	GSOR_SIR_MASK, GSOR_SIR0+GSOR_SIR1+GSOR_SIR2+GSOR_SIR3+GSOR_SIR4+GSOR_SIR5+GSOR_SIR6+GSOR_SIR7
	DEFBT	GSOR_DMAINT,	8	;1=DMA done latch/int
	DEFBT	GSOR_H2BINT,	9	;1=host-to-board int
	DEFBT	GSOR_B2HINT,	10	;1=incomplete host int
	DEFBT	GSOR_FLAG0,	11	;host-to-board flag 0
	DEFBT	GSOR_FLAG1,	12	;		    1
	DEFBT	GSOR_FLAG2,	13	;		    2
	DEFBT	GSOR_FLAG3,	14	;		    3
	DEFBT	GSOR_DMA_IDLE,	15	;1=DMA idle
	DEFBT	GSOR_SIE0,	16	;(write only) SONIC int 0 enabled
	DEFBT	GSOR_SIE1,	17	;			1
	DEFBT	GSOR_SIE2,	18	;			2
	DEFBT	GSOR_SIE3,	19	;			3
	DEFBT	GSOR_SIE4,	20	;			4
	DEFBT	GSOR_SIE5,	21	;			5
	DEFBT	GSOR_SIE6,	22	;			6
	DEFBT	GSOR_SIE7,	23	;			7
	.equ	GSOR_REV,   GSOR_SIE0	;board revision number
	.equ	GSOR_REV_BT,GSOR_SIE0_BT    ;overlaps GSOR_SIE bits
	.equ	GSOR_REV_MASK, GSOR_REV*0xff
	DEFBT	GSOR_DIE,	24	;(read/write) 1=DMA int enabled
	DEFBT	GSOR_HIE,	25	;(read/write) 1=host int enabled


;write-only bits in the INTC
	DEFBT	INTC_DMAINT,GSOR_DMAINT_BT	;1=ack DMA interrupt
	DEFBT	INTC_H2BINT,GSOR_H2BINT_BT	;1=ack host-to-board interrupt
	DEFBT	INTC_B2HINT,GSOR_B2HINT_BT	;1=set board-to-host interrupt


;Interrupts
    ;	.equ	V_SONIC,V_INTR0
    ;	.equ	V_DMA,	V_INTR1
	.equ	V_HOST, V_INTR2
    ;	.equ	V_???,	V_INTR3
	.equ	V_DMA,	V_TRAP0


;DANG "cache line" (not really) size
	DEFBT	DANG_CACHE_SIZE, 7


;SONIC registers
	.equ	SN_CR,	    0x00*4+2	;Command
	.equ	SN_DCR,	    0x01*4+2	;Data Configuration
	.equ	SN_RCR,	    0x02*4+2	;Receive Control
	.equ	SN_TCR,	    0x03*4+2	;Transmit Control
	.equ	SN_IMR,	    0x04*4+2	;Interrupt Mask
	.equ	SN_ISR,	    0x05*4+2	;Interrupts Status
	.equ	SN_DCR2,    0x3f*4+2	;Data Configuration 2

	.equ	SN_UTDA,    0x06*4+2	;Upper Transmit Descriptor Address
	.equ	SN_CTDA,    0x07*4+2	;Current Transmit Descriptor Address

	.equ	SN_URDA,    0x0d*4+2	;Upper Receive Descriptor Address
	.equ	SN_CRDA,    0x0e*4+2	;Current Receive Descriptor Address
	.equ	SN_EOBC,    0x13*4+2	;End of Buffer Word Count
	.equ	SN_URRA,    0x14*4+2	;Upper Receive Resource Address
	.equ	SN_RSA,	    0x15*4+2	;Resource Start Address
	.equ	SN_REA,	    0x16*4+2	;Resource End Pointer
	.equ	SN_RRP,	    0x17*4+2	;Resource Read Pointer
	.equ	SN_RWP,	    0x18*4+2	;Resource Write pointer
	.equ	SN_RSC,	    0x2b*4+2	;Receive Sequence Counter

	.equ	SN_CEP,	    0x21*4+2	;CAM Entry Pointer
	.equ	SN_CAP2,    0x22*4+2	;CAM Address Port 2
	.equ	SN_CAP1,    0x23*4+2	;CAM Address Port 1
	.equ	SN_CAP0,    0x24*4+2	;CAM Address Port 2
	.equ	SN_CE,	    0x25*4+2	;CAM Enable
	.equ	SN_CDP,	    0x26*4+2	;CAM Descriptor Pointer
	.equ	SN_CDC,	    0x27*4+2	;CAM Descriptor Count

	.equ	SN_CRCT,    0x2c*4+2	;CRC Error Tally
	.equ	SN_FAET,    0x2d*4+2	;FAE Tally
	.equ	SN_MPT,	    0x2e*4+2	;Missed Packet Tally

	.equ	SN_WT0,	    0x29*4+2	;Watchdog Timer 0
	.equ	SN_WT1,	    0x2a*4+2	;Watchdog Timer 1

	.equ	SN_SR,	    0x28*4+2	;Silicon Revision


;SONIC Receive Resource Area descriptor, describing an input buffer
	.dsect
_RXrsrc_ptr_lo:	.block	4		;address
_RXrsrc_ptr_hi:	.block	4
_RXrsrc_wc_lo:	.block	4		;length in bytes
_RXrsrc_wc_hi:	.block	4
_SIZE_RXrsrc:	.block	1
	.use	*

;SONIC Receive Descriptor
	.dsect
_RXpkt_status:	.block	4		;packet status
_RXpkt_size:	.block	4		;byte count
_RXpkt_ptr_lo:	.block	4		;start of data
_RXpkt_ptr_hi:	.block	4
_RXpkt_seq_no:	.block	4
_RXpkt_link:	.block	4
	DEFBT	RX_EOL,	    0		;end of list
_RXpkt_inuse:	.block	4
	DEFBT	RX_PTK_INUSE_BUSY,0	;descriptor busy set by host
		.block	4		;EPlex padding to make it a round size
_SIZE_RXpkt:	.block	1
	.use	*

	.equ	RBUF_JUNK_SIZE,	4	;undocumented junk added by SONIC


;SONIC Transmit Descriptor
	.dsect
_TXpkt_status:	.block	4
	DEFBT	TX_COL,	    11		;00  LSB of collision count
	.equ	TX_COL_MASK,0x1f
_TXpkt_config:	.block	4		;04  transmit modes
_TXpkt_psize:	.block	4		;08  packet size in bytes
_TXpkt_frags:	.block	4		;0c  # of fragments in packet
_TXpkt_ptr_lo:	.block	4		;10  start of 1st (& only) fragment
_TXpkt_ptr_hi:	.block	4
_TXpkt_fsize:	.block	4		;18  size of 1st (& only) fragment
_TXpkt_link:	.block	4		;1c  link and EOL bit
	DEFBT	TX_EOL,	    0
_SIZE_TXpkt:	.block	1
	.use	*


;SONIC Command Register
	DEFBT	CR_LCAM,    9		;load CAM
	DEFBT	CR_RRRA,    8		;read RRA
	DEFBT	CR_RST,	    7		;software reset
	DEFBT	CR_ST,	    5		;start timer
	DEFBT	CR_STP,	    4		;stop timer
	DEFBT	CR_RXEN,    3		;receiver enable
	DEFBT	CR_RXDIS,   2		;receiver disable
	DEFBT	CR_TXP,	    1		;transmit packets
	DEFBT	CR_HTX,	    0		;halt transmission


;SONIC Interrupt Status Register
	DEFBT	ISR_BR,	    14		;bus retry occurred
	DEFBT	ISR_HBL,    13		;heartbeat lost
	DEFBT	ISR_LCD,    12		;load CAM done
	DEFBT	ISR_PINT,   11		;programmed interrupt
	DEFBT	ISR_PKTRX,  10		;packet received
	DEFBT	ISR_TXDN,   9		;transmission done
	DEFBT	ISR_TXER,   8		;transmit error
	DEFBT	ISR_TC,	    7		;timer complete
	DEFBT	ISR_RDE,    6		;receive descriptors exhausted
	DEFBT	ISR_RBE,    5		;receive buffer exhausted
	DEFBT	ISR_RBAE,   4		;receive buffer area exhausted
	DEFBT	ISR_CRC,    3		;CRC counter rollover
	DEFBT	ISR_FAE,    2		;frame alignment counter rollover
	DEFBT	ISR_MP,	    1		;missed packet counter rollover
	DEFBT	ISR_RFO,    0		;receive FIFO overrun

    ;input SONIC interrupt bits
	.equ	RX_ISR_BITS,ISR_PKTRX | ISR_RDE | ISR_RBE | ISR_RBAE | ISR_RFO
    ;miscellaneous SONIC interrupt bits
	.equ	MISC_ISR_BITS, ISR_BR | ISR_MP | ISR_TXDN | ISR_PINT | ISR_LCD
    ;all SONIC interrupts we care about
	.equ	IMR_BITS, RX_ISR_BITS | MISC_ISR_BITS


;SONIC Transmit Control Register
	DEFBT	TCR_PINTR,  15		;request programmable interrupt
	DEFBT	TCR_POWC,   14		;late (out of window) collision mode
	DEFBT	TCR_CRCI,   13		;CRC inhibit
	DEFBT	TCR_EXDIS,  12		;disable excessive deferral timer
	DEFBT	TCR_EXD,    10		;excessive deferral
	DEFBT	TCR_DEF,    9		;deferred transmission
	DEFBT	TCR_NCRS,   8		;no carrier sense
	DEFBT	TCR_CRSL,   7		;carrier sense lost
	DEFBT	TCR_EXC,    6		;excessive colllisions
	DEFBT	TCR_OWC,    5		;late (out of window) collision
	DEFBT	TCR_PMB,    3		;packet monitored bad
	DEFBT	TCR_FU,	    2		;FIFO underrun
	DEFBT	TCR_BCM,    1		;byte count mismatch
	DEFBT	TCR_PTX,    0		;packet transmitted ok

;SONIC Receive Control Register
	DEFBT	RCR_ERR,    15		;accept with CRC errors or collsions
	DEFBT	RCR_RNT,    14		;accept runts
	DEFBT	RCR_BRD,    13		;accept broadcast packets
	DEFBT	RCR_PRO,    12		;promiscuouis mode
	DEFBT	RCR_AMC,    11		;accept all multicast
	DEFBT	RCR_LB1,    10		;loopback control
	DEFBT	RCR_LP0,    9
	DEFBT	RCR_MC,	    8		;multicast received
	DEFBT	RCR_BC,	    7		;broadcast received
	DEFBT	RCR_LPKT,   6		;last packet in RBA
	DEFBT	RCR_CRS,    5		;carrier sense activity
	DEFBT	RCR_COL,    4		;collision during reception
	DEFBT	RCR_CRCR,   3		;bad CRC
	DEFBT	RCR_FAER,   2		;frame alignment error
	DEFBT	RCR_LPK,    1		;loopback packet received
	DEFBT	RCR_PRX,    0		;packet received OK

	.equ	_RXpkt_mask, RCR_COL+RCR_CRCR+RCR_FAER+RCR_LPK


;declare a small buffer that can be DMA'ed without worrying about
;   the 4K address restriction in the DMA hardware.
 .macro DMABLK,	lab,len,maxpad
  .if len >= DMA_ALIGN-3
	.print	" ** Error: @len@ is too big"
	.err
  .endif
	BALIGN	DMA_ALIGN,len,maxpad,
  .ifnes "@lab@",""
lab:  .block	  len
  .else
	  .block      len
  .endif
 .endm

;Align to a boundary so loadm/storem will not stutter.
 .macro	MALIGN,	len
	BALIGN	LOADM_ALIGN,len,LOADM_ALIGN/2
 .endm


;align to something
 .macro	BALIGN,	bnd,len,maxpad
  .if (. & -bnd) != ((.+(len)-1) & -bnd)
	.set	THIS_BALIGN, ((0-.) & (bnd-1))
    .if THIS_BALIGN > maxpad+64
	  .set	toobig, (0-.) & (bnd-1)
	  .print " ** Error: wasteful padding"
	  .err
    .endif
	  .block  THIS_BALIGN
	  .set	  TOT_BALIGN, TOT_BALIGN+THIS_BALIGN
  .endif
 .endm
	.set	TOT_BALIGN, 0


;wait for the most recent DMA operation to finish
;   use temporary register tmp
 .macro	WAITDMA,tmp,fs
	  load	  0,WORD,tmp,p_gsor
	  tbit	  tmp,tmp,GSOR_DMA_IDLE
	  jmpf	  tmp,.-2*4
  .ifnes "@fs@","fast"
   .ifnes "@fs@",""
	.print "** Error: @fs@ is not 'fast' or null"
	.err
   .endif
	   nop
  .endif
 .endm


;get address of board regster
 .macro	BADR,	reg,nm
  .if GSOR <= nm
	  add	  reg,p_gsor,nm-GSOR
  .else
	  sub	  reg,p_gsor,GSOR-nm
  .endif
 .endm


;start a DMA operation
;   This assumes the source and destination buffers are word-aligned,
;   the GIO SRAM buffer does not cross a 4K SRAM page boundary,
;   and is an even number of words.
;
;   Use byte count cnt, SRAM address regsram+offsram, and
;	address hosthi:reghost+offhost
 .macro DODMA,	cnt,b2h_h2b, regsram,offsram, hosthi,reghost,offhost, tm1,tm2
  .ifdef DANG_BUG
    .ifeqs "@b2h_h2b@","H2B"
	rs_st	DANG_DMA
    .else
	t_st	tm1,DANG_DMA
	  jmpf	  tm1,$1
	   DUMDMA tm1,tm2
	WAITDMA	tm1,fast
$1:	 set_st	DANG_DMA
    .endif
  .endif
  .if $ISREG(cnt)
	CK	&cnt!=&tm1 && &cnt!=&tm2
  .endif
	WAITDMA	tm1,fast,
	BADR	tm1,DMA_ADDRLO
  .ifeqs "@reghost@",""
	  constx  tm2,offhost
	  store	  0,WORD,tm2,tm1
  .else
    .if offhost+0 != 0
	  add	  tm2,reghost,offhost
	  store	  0,WORD,tm2,tm1
    .else
	  store	  0,WORD,reghost,tm1
    .endif
  .endif
	BADR	tm1,DMA_ADDRHI
	sto	hosthi,tm1,tm2,,
	BADR	tm2,DMA_WC
  .ifeqs "@b2h_h2b@","H2B"
    .if $ISREG(cnt)
	  mov	tm1,cnt
	  consth  tm1,DMA_WC_H2B
    .else
	  constx  tm1,DMA_WC_H2B+cnt
    .endif
	  store	  0,WORD,tm1,tm2
  .else
     .ifnes "@b2h_h2b@","B2H"
	.print	"** Error: @b2h_h2b@ is neither B2H nor H2B"
	.err
     .endif
	sto	cnt,tm2,tm1,,
  .endif
  .if offsram+0==0
	  CK	  (DMA_WC & 0xffff) == (DMA_BC & 0xffff)
  .else
	  const	  tm2,offsram+DMA_BC
  .endif
	  consth  tm2,offsram+DMA_BC
  .ifnes "@regsram@",""
	  add	  tm2,tm2,regsram
  .endif
  .if $ISREG(cnt)
	  store	  0,WORD,cnt,tm2
  .else
    .ifeqs "@b2h_h2b@","H2B"
	  constx  tm1,cnt
    .endif
	  store	  0,WORD,tm1,tm2
  .endif
 .endm


;do a dummy host-to-board DMA operation to flush the "cache" in the DANG
 .macro	DUMDMA,dtmp1,dtmp2
	DODMA	4,H2B, 0,dma_dummy, 0,h2b_pos,0, dtmp1,dtmp2
 .endm
