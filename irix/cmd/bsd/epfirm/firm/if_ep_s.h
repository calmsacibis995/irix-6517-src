;E-Plex 8-port Ethernet firmware interface to the host
;
; Copyright 1990 Silicon Graphics, Inc.  All rights reserved.

;   This must match if_ep.h

; "$Revision: 1.9 $"


;initial 32 bits of message specifying the host communications area
	.equ	EPB_GIO_DL_PREAMBLE, 0x7ffffffe

;shape of the board memory as far as the host knows
    ;download to here
	.equ	EPB_EPROM,	EPROM
	.equ	EPB_GIORAM,	GIORAM
	.equ	EPB_EPROM_SIZE,	EPROM_SIZE

    ;use SRAM starting here for downloading firmware
	.equ	EP_DOWN,	(GIORAM+EPROM_SIZE)

	.equ	EP_FIRM,	EPROM		;the operational firmware
	.equ	EP_FIRM_SIZE,	16*1024

	.equ	EP_DIAG, EP_FIRM+EP_FIRM_SIZE   ;diagnostics
	.equ	EP_DIAG_SIZE,	64*1024

    ;the MAC address lives here
	.equ	EP_MAC_SIZE,	16
	.equ	EP_MACS_ADDR,	(EPROM_SIZE - EP_MAC_SIZE*EP_PORTS)

    ;the EEPROM programming code is put here
	.equ	EP_PROG_SIZE,	(1024*2)
	.equ	EP_PROG,	(EP_DOWN-EP_PROG_SIZE)



;Counters sent to the host for XCMD_FET_CNT
 .macro	CNTS
	CNT	rx_mp			;missed packet
	CNT	rx_hash			;failed to match hash
	CNT	rx_col			;receive collisions
	CNT	rx_babble		;frame too long
	CNT	rx_rfo			;receive FIFO overrun
	CNT	rx_rde			;receive descriptors exhausted
	CNT	rx_rbe			;receive buffers exhausted
	CNT	rx_rbae			;receive buffer area exhausted
	CNT	tx_col			;transmit collisions
	CNT	tx_hbl			;heartbeat missing
	CNT	tx_exd			;excessive deferrals
	CNT	tx_def			;deferrals
	CNT	tx_crsl			;no carrier
	CNT	tx_exc			;excessive collisions
	CNT	tx_owc			;late collisions
	CNT	tx_pmb			;transmission error
	CNT	tx_fu			;FIFO underrun
	CNT	tx_bcm			;byte count mismatch
	CNT	tx_error		;any transmit error
	CNT	links			;LINK bits
	CNT	br			;bus retries
	CNT	pad21
	CNT	pad22
	CNT	pad23
	CNT	pad24
	CNT	pad25
	CNT	pad26
	CNT	pad27
	CNT	pad28
	CNT	pad29
	CNT	pad30
	CNT	pad31
 .endm


;shape of the host-communications area
    ;host-written/board-read portion of the communcations area
	.dsect
hc_cmd:	    .block  4			;one of the epb_cmd enum
hc_cmd_id:  .block  4			;ID used by board to ack cmd

hc_cmd_data:.block  64

	.set	EP_DWN_LEN,8

    ;host-read/board-written part of the communications area

hc_sign:    .block  4			;board signature
	.equ	EPB_SIGN,   0xe8e8f00d
hc_vers:    .block  4			;board/eprom version
	DEFBT	CKSUM_VERS,	31	;bad checksum
    ;after the initial write, the preceding two values are not written

hc_cmd_ack: .block  4			;ID of last completed command

hc_cmd_res: .block  64

    ;MAC_PARMS mod bits
	DEFBT	MMODE_PROM,	0	;promiscuous
	DEFBT	MMODE_AMULTI,	1	;all multicasts for routing

	.equ	MMODE_HLEN, (256/8)	;length of DA hash table in bytes


    ;command read and write lengths
	.equ    HC_CMD_RSIZE, (hc_sign - hc_cmd)
	.equ	HC_CMD_WRES_SIZE, 4	;minimum written to ack a command
	.equ	HC_CMD_SIZE,  (. - hc_cmd)

	.use	*



;Data movement definitions
	.equ	IN_SUM_OFF, 14+20	;start IP cksum after MAC & IP headers

    ;input padding added by board, consisting of 2 bytes of junk to align
    ;the data to 2(mod 4), 2 bytes of TCP checksum, and 2 bytes of status
	.set	RX_PAD,	6

	.set	TX_PAD,	2		;sizeof(ep_out_pad)

	.equ	MIN_BIG,    1
	DEFBT	MAX_BIG,    8		;256 clusters in pool
	.equ	MIN_SML,    1
	DEFBT	MAX_SML,    8		;256 little mbufs in pool

	.equ	RPKT_SIZE, 1518		;maximum input packet
	.equ	RPKT_NET_SIZE, RPKT_SIZE-4  ;without CRC


	.equ	MAX_OUTQ,	64	;per-MAC output queue length


;host-to-board requests
	.dsect				;shape of host-to-board requests
_H2B_OP:    .block	1
_H2B_ADDR_HI:.block	1
_H2B_S1:    .block	2
_H2B_S2:    .block	2
_H2B_S3:    .block	2
_SIZE_H2B_BLK:.block	1
	.use	*

	DEFBT	H2B_ADDR_HI_LSB,16
	DEFBT	H2B_ADDR_HI_MSB,H2B_ADDR_HI_LSB_BT+7
	.equ	H2B_ADDR_HI_MASK, 0xff
	DEFBT	H2B_OP,		24
	.equ	H2B_OP_MASK,	0x3f

	.equ	EPB_H2B_EMPTY,	0
	.equ	EPB_H2B_WRAP,	1	;back to start of buffer
	.equ	EPB_H2B_SML,	2	;add little mbuf to pool
	.equ	EPB_H2B_BIG,	3	;add cluster to pool
	.equ	EPB_H2B_O,	4	;start output (dup EP_PORTS)
	.equ	EPB_H2B_O_CONT,	EPB_H2B_O+EP_PORTS  ;continue output
	.equ	EPB_H2B_O_END,	EPB_H2B_O_CONT+1
	.equ	EPB_H2B_LAST,	EPB_H2B_O_END+1

	.equ	MAX_DMA_ELEM,	3	;maximum chunks in 1 host to board DMA

	DEFBT	H2B_LEN_MSB,	15



;shape of board-to-host control requests
	.dsect
_B2H_ENTRY: .block	4
_SIZE_B2H_BLK:.block	1
	DEFBT	B2H_SERNUM0,24
	DEFBT	B2H_OP0,    20
	DEFBT	B2H_PORT0,  16
	DEFBT	B2H_VAL0,   0
	.use	*

;board-to-host requests in the B2H info
	.equ	B2H_SLEEP,	1*0x0100000	;board has run out of work
	.equ	B2H_ODONE,	2*0x0100000	;output DMA commands finished
	.equ	B2H_IN_SML,	3*0x0100000	;input small packet
	.equ	B2H_IN_BIG,	4*0x0100000	;input big packet

;   Size of board-to-host DMA control info buffer.
;	all available mbufs to be filled with frames
;	all pending output to be completed and reported separately
;	some complaints about going to sleep
;	padding to ensure the length is not a multiple of the period of
;	    the serial number so the serial number always changes
	.equ	B2H_LEN_INS, MAX_SML+MAX_BIG
	.equ	B2H_LEN, (B2H_LEN_INS+MAX_OUTQ*EP_PORTS+1 + 2) | 1
