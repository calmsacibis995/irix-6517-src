;GIO bus FDDI firmware interface to the host
;
; Copyright 1990 Silicon Graphics, Inc.  All rights reserved.

;   This must match if_xpi.h

; "$Revision: 1.22 $"

;approximate speed of the board
	.equ	MHZ, 16

;initial 32 bits of message specifying the host communications area
	.equ	XPI_GIO_DL_PREAMBLE, 0x7ffffffe

;shape of the board memory as far as the host knows
    ;download to here
	.equ	XPI_EPROM,	EPROM
	.equ	XPI_SRAM,	SRAM
	.equ	XPI_EPROM_SIZE,	EPROM_SIZE

    ;use SRAM starting here for downloading firmware
	.equ	XPI_DOWN,	(SRAM+EPROM_SIZE)

	.equ	XPI_FIRM,	0
	.equ	XPI_FIRM_SIZE,	16*1024

    ;the MAC address lives here
	.equ	XPI_MAC_SIZE,	16
	.equ	XPI_MAC_ADDR,	(EPROM_SIZE - XPI_MAC_SIZE)

    ;the host driver is stored here
	.equ	XPI_DRV,	(XPI_FIRM + XPI_FIRM_SIZE)
	.equ	XPI_DRV_SIZE,	(EPROM_SIZE - XPI_FIRM_SIZE - XPI_MAC_SIZE)

    ;the EEPROM programming code is put here
	.equ	XPI_PROG_SIZE,	(1024*2)
	.equ	XPI_PROG,	(XPI_DOWN-XPI_PROG_SIZE)



;Counters sent to the host for XCMD_FET_CNT
 .macro	CNTS
	CNT	ticks,			;   free running clock

	CNT	rngop,			;I  ring went RingOP
	CNT	rngbroke,		;I  ring went non-RingOP
	CNT	pos_dup,		;	our DA with A bit set
	CNT	misfrm,
	CNT	xmtabt,
	CNT	tkerr,			;
	CNT	clm,			;I
	CNT	bec,			;I
	CNT	tvxexp,			;I
	CNT	trtexp,			;I
	CNT	tkiss,			;   token issued
	CNT	myclm,
	CNT	loclm,
	CNT	hiclm,
	CNT	mybec,
	CNT	otrbec,

	CNT	frame_ct,		;   total frames seen
	CNT	tok_ct,			;   token count
	CNT	err_ct,			;   locatable errors
	CNT	lost_ct,		;   format errors

	CNT	lem0_ct,		;   LEM "events" from ELM0
	CNT	lem1_ct,

	CNT	vio0,			;   violation symbols from ELM0
	CNT	vio1,
	CNT	eovf0,			;   elasticity buffer overflows
	CNT	eovf1,
	CNT	elm0st,			;   ELM status bits
	CNT	elm1st,

	CNT	error,			;   error detected by MAC
	CNT	e,			;   received E bit
	CNT	a,			;   received A bit
	CNT	c,			;   received C bit
	CNT	rx_ovf,			;   receive FIFO overrun
	CNT	tx_under,		;   transmit FIFO overrun

	CNT	noise0,			;I  noise events from ELM0
	CNT	noise1,			;I

	CNT	tot_junk,		;   discarded frames
	CNT	shorterr,		;   frame too short
	CNT	longerr,		;   frame too long

	CNT	mac_state,		;   current MAC state

	CNT	dup_mac,		;   likely duplicate address

	CNT	rnglatency,		;   measured ring latency
	CNT	t_neg,			;   recent T_Neg

	CNT	mac_bug,		;I  firmware bugs
	CNT	elm_bug,		;I
	CNT	fsi_bug,		;I
	CNT	mac_cnt_ovf,		;   too much interrupt latency

	CNT	early_rels,		;   released token early
 .endm


;shape of the host-communications area
    ;host-written/board-read portion of the communcations area
	.dsect
hc_cmd:	    .block  4			;one of the xpi_cmd enum
hc_cmd_id:  .block  4			;ID used by board to ack cmd

hc_cmd_data:.block  64

	.set	XPI_DWN_LEN,8

    ;bits in init.flags
	DEFBT	XPI_IN_CKSUM,	0,	;board checksums
	DEFBT	XPI_LAT_VOID,	1,	;enable latency measures
	DEFBT	XPI_ZAPRING,	2,	;mess up the ring
	DEFBT	XPI_IO4IA_WAR,	3,	;combat IO4 IA chip bug


    ;host-read/board-written part of the communications area

hc_sign:    .block  4			;board signature
	.equ	XPI_SIGN,   0xfdd1f00d
hc_vers:    .block  4			;board/eprom version
	DEFBT	CKSUM_VERS,	31	;bad checksum
	DEFBT	BYPASS_VERS,	30	;have an optical bypass
	DEFBT	NO_PHY_VERS,	29	;no PHY installed
	DEFBT	PHY2_VERS,	28	;DAS

    ;after the initial write, the previous two values are not written

hc_inum:    .block  4			;++ on board-to-host interrupt

hc_cmd_ack: .block  4			;ID of last completed command

hc_cmd_res: .block  64			;minimize future version problems

    ;MAC mode requests
	DEFBT	MMODE_PROM,	0	;promiscuous
	DEFBT	MMODE_AMULTI,	1	;all multicasts for routing
	DEFBT	MMODE_HMULT,	2	;CAM is full or want all

	.equ	MMODE_HLEN, (256/8)	;length of DA hash table in bytes


    ;command read and write lengths
	.equ    HC_CMD_RSIZE, (hc_sign - hc_cmd)
	.equ	HC_CMD_WRES_SIZE, 4	;minimum written to ack a command
	.equ	HC_CMD_SIZE,  (. - hc_cmd)

	.use	*



;Data movement definitions
	.equ	IN_SUM_OFF, 44		;start IP cksum after MAC & IP headers
	.equ	IN_CKSUM_SIZE,	4096	;do not bother with fewer data bytes
					;checksum this many bytes at the end

	.equ	IN_PG_SIZE,	4096	;host likes this size aligned

	.set	PAD0_IN_DMA,	24	;input padding added by board
	.set	PADMAX_IN_DMA,	32
	;max runtime changing in padding
	.equ	MAX_DPAD, (PADMAX_IN_DMA-PAD0_IN_DMA)

	.equ	MIN_BIG,    2
	DEFBT	MAX_BIG,    5		;32 clusters in pool
	.equ	MIN_MED,    1
	DEFBT	MAX_MED,    6		;64 medium sized mbufs in pool
	.equ	MIN_SML,    2
	DEFBT	MAX_SML,    6		;64 little mbufs in pool

	.set	MSIZE,	    128
	.set	SML_SIZE0,  112		;little mbufs
	.equ	SML_SIZE1,  256-32
	.equ	MAX_DSML, SML_SIZE1-SML_SIZE0 ;max runtime changing in padding
	.set	MED_SIZE,   (1514+(24-14)+PADMAX_IN_DMA)   ;medium
	.set	BIG_SIZE,   IN_PG_SIZE	;size of big mbufs

	.equ	SML_NET,    SML_SIZE0-PAD0_IN_DMA

;host-to-board requests
	DEFBT	C2B_ADDR_HI_LSB,24
	.equ	C2B_OP_MASK,	(0x1f*4)

	.equ	C2B_EMPTY,  (0*4)
	.equ	C2B_SML,    (1*4)	;add little mbuf to pool
	.equ	C2B_MED,    (2*4)	;add medium mbuf to pool
	.equ	C2B_BIG,    (3*4)	;add cluster to pool
	.equ	C2B_PHY_ACK,(4*4)	;finished PHY interrupt
	.equ	C2B_WRAP,   (5*4)	;back to start of buffer
	.equ	C2B_IDELAY, (6*4)	;delay interrupts
	.equ	C2B_LAST,   (7*4)

	.equ	MAX_DMA_ELEM,	7	;maximum chunks in 1 host to fiber DMA

	.equ	MAX_OUTQ,	64
	.equ	PHY_INT_NUM,	12
	.equ	MININ,		3+13+8	;size of MAC & SNAP input headers


;shape of board-to-host control requests
	.dsect
_B2H_ENTRY: .block	4
_SIZE_B2H_BLK:.block	1
	.use	*

;board-to-host requests in the B2H info
	.equ	B2H_SLEEP,	1*0x010000	;board has run out of work
	.equ	B2H_ODONE,	2*0x010000	;output DMA commands finished
	.equ	B2H_IN,		3*0x010000	;input DMA finished
	.equ	B2H_IN_DONE,	4*0x010000
	.equ	B2H_PHY,	5*0x010000	;PHY interrupt

;   Size of board-to-host DMA control info buffer.
;	one entry for each input buffer,
;	one entry for each possible "SLEEP" announcement, which is
;		one for each input done plus one
;	one entry for each possible output-done announcment, times two
;		for historical reasons and since we cannot change the
;		length now
;	one entry for each pending PHY interrupt,
;	empty fences
;	made odd to not be a power of 2
	.equ	B2H_LEN_INS, MAX_SML+MAX_MED+MAX_BIG
	.equ	B2H_LEN, (B2H_LEN_INS+1+MAX_OUTQ*2+1+PHY_INT_NUM+2) | 1

;shape of host-to-board data block requests
	.dsect
_D2B_OP:    .block	4
_D2B_ADDR:  .block	4
_SIZE_D2B_BLK:.block	1
	.use	*

;bits in data-to-board requests
	DEFBT	D2B_RDY,	31	;start of next request
	DEFBT	D2B_BAD,	30
	DEFBT	D2B_UNKNOWN,	0

	DEFBT	D2B_ADDR_HI_LSB,16
	DEFBT	D2B_LEN_MSB,	15


;Board type bits
	DEFBT	BD_TYPE_PHY2,	0	;have 2nd phy
	DEFBT	BD_TYPE_MAC2,	1	;connected to 2nd board
	DEFBT	BD_TYPE_BYPASS,	2	;seem to have an optical bypass


;These values must match those in fddi.h

	DEFBT	MAC_E_BIT,	4
	DEFBT	MAC_A_BIT,	3
	DEFBT	MAC_C_BIT,	2
	DEFBT	MAC_ERROR_BIT,	1
	DEFBT	MAC_DA_BIT,	0

    ;Official line states
	.equ	PCM_QLS, 0
	.equ	PCM_ILS, 1
	.equ	PCM_MLS, 2
	.equ	PCM_HLS, 3
	.equ	PCM_ALS, 4
	.equ	PCM_ULS, 5
	.equ	PCM_NLS, 6
	.equ	PCM_LSU, 7
    ;other states, from the ELM
	.equ	RCV_TRACE_PROP,	8
	.equ	RCV_SELF_TEST,	9
	.equ	RCV_BREAK,	10
	.equ	RCV_PCM_CODE,	11

;define an ELM timer value, in 256*80 nsec units, from a number of usec
 .macro	DLONGT,	lab, val
	.equ	lab, -(((val)*100+8*256-1)/(8*256))
 .endm
;define an ELM timer value, in 4*80 nsec units, from a number of usec
 .macro	DST,	lab, val
	.equ	lab, -(((val)*100)/(8*4))
 .endm
	DLONGT	SMT_C_MIN,  2000	;stay in CONNECT this long
	DLONGT	SMT_TL_MIN, 1000	;transmit for this long
					;slowly to ensure others keep up
	DLONGT	SMT_TB_MIN, (5*1000)	;minimum BREAK time
	DLONGT	SMT_T_OUT,  (100*1000)	;signalling timeout
	DST	SMT_NS_MAX, 1311	;1.31072 msec
	DLONGT	SMT_T_SCRUB,3000	;longer than ring latency
	.purgem	DLONT, DST


    ;Official MAC states
	.equ	MAC_OFF,    0
	.equ	MAC_IDLE,   1
	.equ	MAC_CLAIMING,2
	.equ	MAC_BEACONING,3
	.equ	MAC_ACTIVE, 4

    ;MAC FC types
	.equ	MAC_FC_BEACON,	0xc2

    ;MAC standard value, in microseconds
	.equ	D_Max, 2661
