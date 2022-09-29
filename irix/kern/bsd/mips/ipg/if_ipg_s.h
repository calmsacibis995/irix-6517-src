;Interface to the FDDIXPress "firmware"
;
; Copyright 1990 Silicon Graphics, Inc.  All rights reserved.
;	"$Revision: 1.28 $"

;This definitions in this file must match the corresponding values in
;	if_ipg.h

 .macro	ALIAS,  oname			;ready to define an alias
	.set	DSIO_PREV, oname-SIO_INCR
 .endm


;The shape of the "short I/O" board RAM, as seen by the host
;   This is preceded by the short words defined in eprom.h.

	DSIO	SIO_ACT_DATA

	;FORMAC parameters
	ALIAS	SIO_ACT_DATA
	DSIO	SIO_T_MAX
	DSIO	SIO_TVX

	;VME parameters
	DEFBT	IPG_LAT,	0,	;enable latency measures


	;Counters and "events"
	ALIAS	SIO_ACT_DATA
 .macro	CNT,	mnam			;define a counter
	CK	0 == ((DSIO_PREV+SIO_INCR-SIO_ACT_DATA) & (SIO_INCR*2-1))
	ASIO	,2,
 .endm

 .macro	CNTRS
	CNT	ticks,			;free running timer

	CNT	rngop,			;ring went RingOP
	CNT	rngbroke,		;ring went non-RingOP
	CNT	pos_dup,
	CNT	misfrm,
	CNT	xmtabt,
	CNT	tkerr,
	CNT	clm,
	CNT	bec,
	CNT	tvxexp,
	CNT	trtexp,
	CNT	tkiss,			;token issued
	CNT	myclm,
	CNT	loclm,
	CNT	hiclm,
	CNT	mybec,
	CNT	otrbec,

	CNT	frame_ct,		;total frames seen
	CNT	tok_ct,			;token count
	CNT	err_ct,			;FORMAC error pulses
	CNT	lost_ct,		;FORMAC lost pulses

	CNT	lem1_ct,		;LEM "events"
	CNT	lem1_usec4,		;usec*4 of elapsed measuring time
	CNT	lem2_ct,
	CNT	lem2_usec4,

	CNT	eovf1,			;elasticity overflows
	CNT	eovf2,

	CNT	flsh,			;flushed frames
	CNT	abort,			;aborted frames
	CNT	miss,			;gap too small
	CNT	error,			;error detected by MAC
	CNT	e,			;received E bit
	CNT	a,			;received A bit
	CNT	c,			;received C bit
	CNT	rx_ovf,			;receive FIFO overrun
	CNT	buf_ovf,		;out of receive buffers
	CNT	tx_under,		;transmit FIFO overrun

	CNT	noise1,			;noise events
	CNT	noise2,

	CNT	tot_junk,		;discarded frames
	CNT	junk_void,		;discarded void frames
	CNT	junk_bec,		;discarded beacon frames
	CNT	shorterr,		;frame too short
	CNT	longerr,		;frame too long

	CNT	mac_state,		;current MAC state

	CNT	dup_mac,		;likely duplicate address

	CNT	rnglatency,		;measured ring latency in usec/16
 .endm
	CNTRS


	.equ	ACT_DONE, 1		;when finished with a host command

;flags to host describing start-up woes
	.equ	ACT_FLAG_MCAST, 0x4000	;missing multicast hardware
	.equ	ACT_FLAG_CKSUM,	0x4001	;bad firmware checksum

	CK	DSIO_PREV>SIO_BASE
	CK	DSIO_PREV<=SIO_BASE+SIO_SIZE

	.purgem	DSIO, ASIO, CNT



	.equ	SUM_OFF, 44		;start IP cksum after MAC & IP headers
	.equ	IN_CKSUM_LEN,	4096	;do not bother with fewer data bytes
					;checksum this many bytes at the end

	.equ	IN_PG_SIZE,	4096	;host likes this size aligned

	.set	PAD0_IN_DMA,	24	;input padding added by board
	.set	PADMAX_IN_DMA,	32
	;max runtime changing in padding
	.equ	MAX_DPAD, (PADMAX_IN_DMA-PAD0_IN_DMA)


    ;The minimum numbers of buffers depends on the maximum number of
    ;buffers that a single incoming frame might use.
	DEFBT	MAX_BIG,    5		;32 clusters in pool
	.equ	MIN_BIG,    2
	DEFBT	MAX_MED,    5		;32 medium sized mbufs in pool
	.equ	MIN_MED,    1
	DEFBT	MAX_SML,    6		;64 little mbufs in pool
	.equ	MIN_SML,    2

	.equ	SML_SIZE0,  112		;"little mbuf"
	.equ	SML_SIZE1,  256-32
	.equ	MAX_DSML, SML_SIZE1-SML_SIZE0 ;max runtime changing in padding
	.equ	MED_SIZE,   (1514+(24-14)+PADMAX_IN_DMA)	;medium
	.equ	BIG_SIZE,   IN_PG_SIZE	;size of big mbufs

	.equ	SML_NET,    SML_SIZE0-PAD0_IN_DMA

;host-to-board requests
	.equ	H2B_EMPTY,	(0*4)
	.equ	H2B_SMLBUF,	(1*4)	;add little mbuf to pool
	.equ	H2B_MEDBUF,	(2*4)	;add medium mbuf to pool
	.equ	H2B_BIGBUF,	(3*4)	;add cluster to pool
	.equ	H2B_OUT,	(4*4)	;start output DMA chain
	.equ	H2B_OUT_FIN,	(5*4)	;end output DMA chain
	.equ	H2B_PHY_ACK,	(6*4)	;finished PHY interrupt
	.equ	H2B_WRAP,	(7*4)	;back to start of buffer
	.equ	H2B_IDELAY,	(8*4)	;delay interrupts

	.equ	MAX_DMA_ELEM, 6		;maximum chunks in 1 host to fiber DMA

	.equ	IFQ_MAXLEN, 50
	.equ	PHY_INT_NUM, 12
	.equ	MININ, 3+13+8		;size of MAC & SNAP input headers

;board-to-host requests in the B2H info
	.equ	B2H_SLEEP,	1*0x010000	;board has run out of work
	.equ	B2H_ODONE,	2*0x010000	;output DMA commands finished
	.equ	B2H_IN,		3*0x010000	;input DMA finished
	.equ	B2H_IN_DONE,	4*0x010000
	.equ	B2H_PHY,	5*0x010000	;PHY interrupt


;   Size of board-to-host control info buffer.
;	one entry for each input buffer,
;	one entry for each possible "SLEEP" announcement, which is
;	    one for each input buffer
;	one entry for each possible output-done announcment
;	one entry for each pending PHY interrupt,
;	empty fences
	.equ	B2H_LEN,((MAX_SML+MAX_MED+MAX_BIG)*2+IFQ_MAXLEN+PHY_INT_NUM+2)

;Board type bits
	DEFBT	BD_TYPE_MAC2,	0	;connected to 2nd board
	DEFBT	BD_TYPE_BYPASS,	1	;seem to have an optical bypass



;Official line states
;   This must match the PCM_LS enum in fddi.h
	.equ	PCM_QLS, 0
	.equ	PCM_ILS, 1
	.equ	PCM_MLS, 2
	.equ	PCM_HLS, 3
	.equ	PCM_ALS, 4
	.equ	PCM_ULS, 5
	.equ	PCM_NLS, 6
	.equ	PCM_LSU, 7

	.equ	SMT_NS_MAX, 1311


;   This must match the mac_states enum in fddi.h
	.equ	MAC_OFF,    0
	.equ	MAC_IDLE,   1
	.equ	MAC_CLAIMING,2
	.equ	MAC_BEACONING,3
	.equ	MAC_ACTIVE, 4

;   MAC FC types
	.equ	MAC_FC_VOID,	0	;void frame FC, without length bit
	.equ	MAC_FC_VOID_MASK,0xbf
	.equ	MAC_FC_LLC_L,	0x10+0x40+1	;long address LLC frame FC
	.equ	MAC_FC_BEACON,	0xc2

    ;MAC standard value, in microseconds
	.equ	D_Max, 2661
	.equ	T_Max, 165000

    ;so host can recognize bad values
	.equ	MAX_RNG_LATENCY, D_Max*MHZ

