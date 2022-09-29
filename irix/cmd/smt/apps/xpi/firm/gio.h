;hardware definitions for GIO bus FDDI board
;
; Copyright 1992 Silicon Graphics, Inc.  All rights reserved.
;	"$Revision: 1.24 $"

;LED meanings.
;      duration    red	   green
;	< 50msec    off	    off	    execution started
;		    off	    off	    Bad EPROM checksum
;		    on		    Ring-OP false
;			    on	    LLC traffic


 .ifdef MEZ
	DEFBT	DANG_CACHE_SIZE,7	;size of DANG "cache" line
	DEFBT	IO4_CACHE_SIZE,	7	;size of IO4 cache line
 .endif

;board address space
	.equ	EPROM,	    0x000000	;start of EEPROM
	.equ	EPROM_SIZE, 128*1024
	.equ	EPROM_WRITE,0x200000	;program it starting here
	.equ    SRAM,	    0x400000	;start of SRAM
	.equ	SRAM_SIZE,  256*1024
	.equ    CRAP,	    0x600000	;DMA local initial address
	.equ    BLUR,	    0x600004	;DMA count, LEDs, etc.
	.equ    VSAR,	    0x600008	;DMA host initial address
	.equ    GSOR,	    0x60000c	;GIO slave operations register
	.equ	CREG,	    0x700000	;Configuration

;bits in the CRAP register
 .ifdef MEZ
	DEFBT	CRAP_EB_LSB,	24	;8 bits of EBus address
 .endif
    ;	DEFBT	CRAP_SRAM_LSB,	0	;16 bits of SRAM address


;bits in the BLUR register
;   write-only by the 29K
 .ifdef LC
	DEFBT	BLUR_MBA1,	31	;master starting address bit 1
	DEFBT	BLUR_MBA0,	30	;master starting address bit 0
    ;	DEFBT	BLUR_???,	29
	DEFBT	BLUR_MBA2,	28	;master starting address bit 2

	DEFBT	BLUR_LE,	27	;little ending packing
	DEFBT	BLUR_SUBBLK_ORDER,26	;transfer in cache-block order
	DEFBT	BLUR_BACKWARD,	25	;DMA backwards
	DEFBT	BLUR_DEV_ID_LSB,19	;GIO device ID
	.equ	BLUR_DEV_ID_MASK, BLUR_DEV_ID_LSB*0x3f
 .endif
 .ifdef MEZ
	DEFBT	BLUR_GIO_BUSY,	25	;1=other FDDI device using GIO
 .endif

;   read-write by the 29K
	DEFBT	BLUR_C_MSB,	9	;most signficant count bit
	.equ	BLUR_MAX_C, BLUR_C_MSB	;max DMA byte count
	DEFBT	DMA_ALIGN,	9	;must align board buffers to this
	DEFBT	BLUR_C_LSB,	2	;least significant count bit
	.equ	BLUR_ROUND, BLUR_C_LSB-1    ;round DMA length up

;   read-only by the 29K
	DEFBT	BLUR_CONF_LSB,	30,	;daughter card configuration
	.equ	BLUR_CONF_MASK,	3
	.equ	BLUR_SAS,	0	    ;SAS
	.equ	BLUR_DAS,	3	    ;DAS-SM
	.equ	BLUR_CDDI,	1	    ;SAS CDDI
	DEFBT	BLUR_BYPASS_PRES,29	;1=have an optical bypass
 .ifdef MEZ
	DEFBT	BLUR_PHY_PRES,	28	;1=daughter card present
	DEFBT	BLUR_DEV_ID,	27	;ID of this half of the board
	DEFBT	BLUR_MB_CONFIG,	26	;1=FDDI device #1 present
	DEFBT	BLUR_GIO_OWNER,	25	;1=other FDDI using GIO bus
 .endif


;write-only bits in CREG
	DEFBT	CREG_B2H,	28	;1=next DMA will be board-to-host

	DEFBT	CREG_BYPASS,	27	;1=turn on bypass switch
	DEFBT	CREG_ZEN,	26	;1=enable interrupt on DMA count 0
	DEFBT	CREG_GREEN,	25	;0=turn on green LED
	DEFBT	CREG_RED,	24	;0=turn on red LED


	;bits in the GSOR
;   written by 29k
	DEFBT	GSOR_INT0,  0
 .ifdef LC
	DEFBT	GSOR_INT1,  1
	DEFBT	GSOR_INT2,  2
 .endif
;   read by the 29k
	DEFBT	GSOR_FLAG0, 1
	DEFBT	GSOR_FLAG1, 2
	DEFBT	GSOR_FLAG2, 3
	DEFBT	GSOR_FLAG3, 4
	.equ	GSOR_MASK, 0x1e
	DEFBT	GSOR_H2BINT,5		;host-to-board interrupt active

 .ifdef LC
    ;we are forced to use interrupt #1 by graphics
	.equ	GSOR_HINT,GSOR_INT1
	.equ	GSOR_HINT_BT,GSOR_INT1_BT
 .endif
 .ifdef MEZ
    ;mez. board has only one interrupt
	.equ	GSOR_HINT,GSOR_INT0
	.equ	GSOR_HINT_BT,GSOR_INT0_BT
 .endif


;Interruts
	.equ	V_HOST,V_INTR0
	.equ	V_FSI, V_INTR1
	.equ	V_MAC, V_INTR2
	.equ	V_ELM, V_INTR3
	.equ	V_DMA, V_TRAP0

;FSI addresses
	.equ	FSI_GSR,    0x00500004
	.equ	FSI_IOR,    0x0050000c
	.equ	FSI_IMR,    0x00500014
	.equ	FSI_PSR,    0x0050001c
	.equ	FSI_CMR,    0x00500024
	.equ	FSI_CER,    0x0050002c
	.equ	FSI_FCR,    0x0050003c

	.equ	FSI_RING_LEN,	8
	.equ	FSI_RING_ADDR,	4	    ;offset to address in descriptor

    ;FSI internal registers reached through the FCR
	DEFBT	FSI_R,	    31
	DEFBT	FSI_IRT,    27
	DEFBT	FSI_IRI,    24
	DEFBT	FSI_DATA,   16
	.equ	FSI_PCRA,   0xb*FSI_IRT | 6*FSI_IRI	    ;port A control
	.equ	FSI_PCRB,   0xb*FSI_IRT | 7*FSI_IRI	    ;port B control
	.equ	FSI_PMPA,   0x8*FSI_IRT | 6*FSI_IRI	    ;port A mem page
	.equ	FSI_PMPB,   0x8*FSI_IRT | 7*FSI_IRI	    ;port B mem page
	.equ	FSI_RPR,    0x6*FSI_IRT			    ;ring parameters
	.equ	FSI_RSR,    0xe*FSI_IRT | FSI_R		    ;ring states
	.equ	FSI_FWR,    0x4*FSI_IRT			    ;FIFO watermarks
	  .equ	  FSI_FWR_MIN,	2
	  .equ	  FSI_FWR_BLK,	32
	.equ	FSI_LMT,    0x5*FSI_IRT			    ;limit
	.equ	FSI_RFR,    0x1*FSI_IRT			    ;RX frame types
	  .equ	  FSI_RFR_DATA, 0x3a	    ;OT,LS,LA,SF
	  .equ	  FSI_RFR_MAC,	0x4	    ;MA
	.equ	FSI_RBR,    0x3*FSI_IRT			    ;RX buffer lengths
	  DEFBT	  FSI_RBR_MIN,  6
	.equ	FSI_RMR,    0xc*FSI_IRT			    ;RX mem space
	  .equ	  FSI_RMR_MIN,	10
	  .equ	  FSI_RMR_MAX,	255
	.equ	FSI_MTR,    0x1*FSI_IRT			    ;MACIF TX control
	.equ	FSI_MRR,    0x1*FSI_IRT | 1*FSI_IRI	    ;MACIF RX control
	.equ	FSI_RDY,    0x0*FSI_IRT			    ;ring ready
	.equ	FSI_IER,    0xf*FSI_IRT	| 0*FSI_IRI	    ;error status
	.equ	FSI_REV,    0xf*FSI_IRT | 2*FSI_IRI	    ;revision
	.equ	FSI_RESET,  0xf*FSI_IRT | 7*FSI_IRI | FSI_R ;FSI reset

    ;bits in the internal FSI register PCR (port control register)
	DEFBT	FSI_PCR_HPE,	7
	DEFBT	FSI_PCR_PC,	6
	DEFBT	FSI_PCR_SO,	5
	DEFBT	FSI_PCR_DO,	4
	DEFBT	FSI_PCR_DW,	2
	DEFBT	FSI_PCR_PE,	0

    ;values for the internal FSI register PMP (port memory page register)
	.equ	FSI_PMP_16,	0
	.equ	FSI_PMP_32,	1
	.equ	FSI_PMP_256,	2
	.equ	FSI_PMP_512,	3
	.equ	FSI_PMP_1K,	4
	.equ	FSI_PMP_2K,	5
	.equ	FSI_PMP_4K,	6
	.equ	FSI_PMP_16K,	7

    ;bits in the internal FSI register RPR (ring parameter register)
	DEFBT	FSI_RPR_RPE,	7	;Ring Parity Enable
	DEFBT	FSI_RPR_DPE,	6	;Data Parity Enable
	DEFBT	FSI_RPR_RPA,	5	;Ring Port Assignment
	DEFBT	FSI_RPR_RDA,	4	;Ring Data Assignment

    ;bits in the internal FSI register RSR (ring state register)
	DEFBT	FSI_RSR_EX,	7	;exists
	DEFBT	FSI_RSR_PER,	6	;parity error
	DEFBT	FSI_RSR_OER,	5	;operation error
	DEFBT	FSI_RSR_STP,	3	;stopped state
	DEFBT	FSI_RSR_RDY,	2	;ready state
	DEFBT	FSI_RSR_EMP,	1	;empty state
	DEFBT	FSI_RSR_CPL,	0	;complete state

    ;bits in the internal FSI register RFR (rcv frame type register)
	DEFBT	FSI_RFR_TE,	7
	DEFBT	FSI_RFR_OT,	5
	DEFBT	FSI_RFR_LS,	4
	DEFBT	FSI_RFR_LA,	3
	DEFBT	FSI_RFR_MF,	2
	DEFBT	FSI_RFR_SF,	1
	DEFBT	FSI_RFR_VF,	0

    ;bits in the internal FSI register MRR (MAC reeceive control register)
	DEFBT	FSI_MRR_RPE,	7
	DEFBT	FSI_MRR_RMI,	5
	DEFBT	FSI_MRR_RAL,	4
	DEFBT	FSI_MRR_RCM,	3
	DEFBT	FSI_MRR_ROB,	2
	DEFBT	FSI_MRR_RE5,	1
	DEFBT	FSI_MRR_RE4,	0

    ;bits in the internal FSI register IER (Internal Error Status register)
	DEFBT	FSI_IER_IOE,	7
	DEFBT	FSI_IER_IUE,	6
	DEFBT	FSI_IER_TPE,	5
	DEFBT	FSI_IER_MER,	4
	DEFBT	FSI_IER_MOV,	3
	DEFBT	FSI_IER_PBE,	1
	DEFBT	FSI_IER_PAE,	0


	.equ	FSI_CAM_LEN, 32	    ;32 8-byte entries

    ;FSI GSR bits
	DEFBT	GSR_CDN,    31
	DEFBT	GSR_CRF,    30
	DEFBT	GSR_HER,    29
	DEFBT	GSR_IOE,    28

	DEFBT	GSR_ROV5,   27
	DEFBT	GSR_ROV4,   26
	DEFBT	GSR_POEB,   25
	DEFBT	GSR_POEA,   24

	DEFBT	GSR_RER5,   21
	DEFBT	GSR_RER4,   20

	DEFBT	GSR_RER3,   19
	DEFBT	GSR_RER2,   18
	DEFBT	GSR_RER1,   17
	DEFBT	GSR_RER0,   16

	DEFBT	GSR_RXC5,   13
	DEFBT	GSR_RXC4,   12

	DEFBT	GSR_RCC3,   11
	DEFBT	GSR_RCC2,   10
	DEFBT	GSR_RCC1,   9
	DEFBT	GSR_RCC0,   8

	DEFBT	GSR_RNR5,   5
	DEFBT	GSR_RNR4,   4

	DEFBT	GSR_RNR3,   3
	DEFBT	GSR_RNR2,   2
	DEFBT	GSR_RNR1,   1
	DEFBT	GSR_RNR0,   0

	.set	m,   GSR_HER
	.set	m,m| GSR_IOE
	.set	m,m| GSR_POEB
	.set	m,m| GSR_POEA
	.set	m,m| GSR_RER5
	.set	m,m| GSR_RER4
	.set	m,m| GSR_RER3
	.set	m,m| GSR_RER2
	.set	m,m| GSR_RER1
	.set	m,m| GSR_RER0
	.equ	GSR_ERRORS, m


    ;FSI commands and indications
	DEFBT	FSI_CMD_OWN,	63-32	    ;"own" bit
	DEFBT	FSI_CMD_U,	62-32	    ;"user defined" bit
	DEFBT	FSI_CMD_F,	61-32	    ;"first" bit
	DEFBT	FSI_CMD_L,	60-32	    ;"last" bit

	.equ	FSI_CMD_NOP,	0xbf000000
	.equ	FSI_CMD_CRW,	0xb6000000  ;control register write
	.equ	FSI_CMD_DEFR,	0xbe000000  ;define ring
	.equ	FSI_CMD_DEFRDY, 0xbe080000  ;define ring and make it ready
	.equ	FSI_CMD_RSTOP,	0xbe200000  ;stop ring
	.equ	FSI_CMD_RPARM,	0xbe800000  ;read ring parameters
	.equ	FSI_CMD_RRESET,	0xbea00000  ;ring reset

	.equ	FSI_CMD_CLR_CAM,0xb9000000  ;clear CAM
	.equ	FSI_CMD_SET_CAM,0xb9800000  ;set CAM
	.equ	FSI_CMD_RD_CAM,	0xba000000  ;read CAM
	.equ	FSI_CMD_CMP_CAM,0xbb800000  ;probe CAM

	DEFBT	FSI_CMD_RING_N,	    48-32

	DEFBT	FSI_IND_RCV_ERR,    58-32   ;receive error
	DEFBT	FSI_IND_RCV_OE,	    57-32   ;overrun error in the FSI
	DEFBT	FSI_IND_RCV_CRC,    56-32   ;CRC (or parity) error
	DEFBT	FSI_IND_FNUM_MSB,   55-32   ;# of flag bits
	DEFBT	FSI_IND_FNUM_LSB,   53-32
	.equ	FSI_IND_FNUM_MASK,  7*FSI_IND_FNUM_LSB
	.equ	FSI_IND_FNUM_MIN,   3*FSI_IND_FNUM_LSB
	DEFBT	FSI_IND_RCV_E,	    52-32   ;E flag
	DEFBT	FSI_IND_RCV_A,	    51-32   ;A flag
	DEFBT	FSI_IND_RCV_C,	    50-32   ;C flag
	DEFBT	FSI_IND_RCV_DA_MSB, 47-32   ;destination address match field
	DEFBT	FSI_IND_RCV_DA_LSB, 46-32
	.equ	FSI_IND_RCV_LEN_MASK,   0x1fff  ;frame length

	.set	m,   FSI_IND_RCV_CRC
	.set	m,m| FSI_IND_RCV_E
	.set	m,m| FSI_IND_RCV_A
	.set	m,m| FSI_IND_RCV_C
	.equ	FSI_IND_RCV_CNT, m	    ;something to count

	DEFBT	FSI_IND_TX_PPE,	    52-32   ;port parity error
	DEFBT	FSI_IND_TX_PAE,	    51-32   ;port abort error
	DEFBT	FSI_IND_TX_AB,	    50-32   ;TX Abort
	DEFBT	FSI_IND_TX_UN,	    49-32   ;underrun
	DEFBT	FSI_IND_TX_PE,	    48-32   ;party error

	.set	m,   FSI_IND_TX_PPE
	.set	m,m| FSI_IND_TX_PAE
	.set	m,m| FSI_IND_TX_AB
	.set	m,m| FSI_IND_TX_UN
	.set	m,m| FSI_IND_TX_PE
	.equ	FSI_IND_TX_BAD, m	    ;frame was aborted

	.equ	FSI_CMD_TX_LEN_MASK, 0xffff


;ELM addresses
;   Each is 16 bits, and so expect to use HWORD loads and stores
	.equ	ELM0_CTRL_A,	0x00500202
	.equ	ELM0_CTRL_B,	0x00500206
	.equ	ELM0_MASK,	0x0050020a
	.equ	ELM0_XMIT_VEC,	0x0050020e
	.equ	ELM0_XMIT_LEN,	0x00500212
	.equ	ELM0_LE_THRESH,	0x00500216
	.equ	ELM0_A_MAX,	0x0050021a
	.equ	ELM0_LS_MAX,	0x0050021e
	.equ	ELM0_TB_MIN,	0x00500222
	.equ	ELM0_T_OUT,	0x00500226
	.equ	ELM0_CIPHER,	0x0050022a
	.equ	ELM0_LC_SHORT,	0x0050022e
	.equ	ELM0_T_SCRUB,	0x00500232
	.equ	ELM0_NS_MAX,	0x00500236
	.equ	ELM0_TPC_LOAD,	0x0050023a
	.equ	ELM0_TNE_LOAD,	0x0050023e
	.equ	ELM0_STATUS_A,	0x00500242
	.equ	ELM0_STATUS_B,	0x00500246
	.equ	ELM0_TPC,	0x0050024a
	.equ	ELM0_TNE,	0x0050024e
	.equ	ELM0_CLK_DIV,	0x00500252
	.equ	ELM0_BIST_SIG,	0x00500256
	.equ	ELM0_RCV_VEC,	0x0050025a
	.equ	ELM0_EVENT,	0x0050025e
	.equ	ELM0_VIOL_SYM,	0x00500262
	.equ	ELM0_MIN_IDLE,	0x00500266
	.equ	ELM0_LINK_ERR,	0x0050026a
	.equ	ELM0_FOTOFF_ON,	0x0050027a
	.equ	ELM0_FOTOFF_OFF,0x0050027e

	.equ	ELM1_CTRL_A,	0x00500302
	.equ	ELM1_CTRL_B,	0x00500306
	.equ	ELM1_MASK,	0x0050030a
	.equ	ELM1_XMIT_VEC,	0x0050030e
	.equ	ELM1_XMIT_LEN,	0x00500312
	.equ	ELM1_LE_THRESH,	0x00500316
	.equ	ELM1_A_MAX,	0x0050031a
	.equ	ELM1_LS_MAX,	0x0050031e
	.equ	ELM1_TB_MIN,	0x00500322
	.equ	ELM1_T_OUT,	0x00500326
	.equ	ELM1_CIPHER,	0x0050032a
	.equ	ELM1_LC_SHORT,	0x0050032e
	.equ	ELM1_T_SCRUB,	0x00500332
	.equ	ELM1_NS_MAX,	0x00500336
	.equ	ELM1_TPC_LOAD,	0x0050033a
	.equ	ELM1_TNE_LOAD,	0x0050033e
	.equ	ELM1_STATUS_A,	0x00500342
	.equ	ELM1_STATUS_B,	0x00500346
	.equ	ELM1_TPC,	0x0050034a
	.equ	ELM1_TNE,	0x0050034e
	.equ	ELM1_CLK_DIV,	0x00500352
	.equ	ELM1_BIST_SIG,	0x00500356
	.equ	ELM1_RCV_VEC,	0x0050035a
	.equ	ELM1_EVENT,	0x0050035e
	.equ	ELM1_VIOL_SYM,	0x00500362
	.equ	ELM1_MIN_IDLE,	0x00500366
	.equ	ELM1_LINK_ERR,	0x0050036a
	.equ	ELM1_FOTOFF_ON,	0x0050037a
	.equ	ELM1_FOTOFF_OFF,0x0050037e


    ;ELM control register A, ELM*_CTRL_A
	DEFBT	ELM_CA_NOISE_TIMER,	14
	DEFBT	ELM_CA_TNE_16BIT,	13
	DEFBT	ELM_CA_TPC_16BIT,	12
	DEFBT	ELM_CA_REQ_SCRUB,	11
	DEFBT	ELM_CA_ENA_PAR_CHK,	10
	DEFBT	ELM_CA_VSYM_CTR_INTRS,	9
	DEFBT	ELM_CA_MINI_CTR_INTRS,	8
	DEFBT	ELM_CA_FCG_LOC_LOOP,	7
	DEFBT	ELM_CA_FOT_OFF,		6
	DEFBT	ELM_CA_EB_LOC_LOOP,	5
	DEFBT	ELM_CA_LOC_LOOP,	4
	DEFBT	ELM_CA_SC_BYPASS,	3
	DEFBT	ELM_CA_REM_LOOP,	2
	DEFBT	ELM_CA_RF_DISABLE,	1
	DEFBT	ELM_CA_RUN_BIST,	0

	.set	m,   ELM_CA_NOISE_TIMER
	.set	m,m| ELM_CA_VSYM_CTR_INTRS
	.set	m,m| ELM_CA_MINI_CTR_INTRS
	.equ	ELM_CA_DEFAULT, m

    ;ELM control register B, ELM*_CTRL_B
	DEFBT	ELM_CB_CONFIG_CNTRL,	15
	DEFBT	ELM_CB_MATCH_LS_LSB,	11
	.equ	ELM_CB_MATCH_LS_MASK,	0xf*ELM_CB_MATCH_LS_LSB
	DEFBT	ELM_CB_MATCH_QLS,	14
	DEFBT	ELM_CB_MATCH_MLS,	13
	DEFBT	ELM_CB_MATCH_HLS,	12
	DEFBT	ELM_CB_MATCH_ILS,	11
	DEFBT	ELM_CB_LS_TX_LSB,	8
	.equ	ELM_CB_LS_TX_MASK,	7*ELM_CB_LS_TX_LSB
	.equ	ELM_CB_TX_QLS,		0*ELM_CB_LS_TX_LSB
	.equ	ELM_CB_TX_ILS,		1*ELM_CB_LS_TX_LSB
	.equ	ELM_CB_TX_HLS,		2*ELM_CB_LS_TX_LSB
	.equ	ELM_CB_TX_MLS,		3*ELM_CB_LS_TX_LSB
	.equ	ELM_CB_TX_ALS,		6*ELM_CB_LS_TX_LSB
	DEFBT	ELM_CB_CLASS_S,		7
	DEFBT	ELM_CB_PC_LOOP_LSB,	5
	.equ	ELM_CB_PC_LOOP_MASK,	3*ELM_CB_PC_LOOP_LSB
	.equ	ELM_CB_PC_LOOP,		3*ELM_CB_PC_LOOP_LSB
	DEFBT	ELM_CB_PC_JOIN,		4
	DEFBT	ELM_CB_LONG,		3
	DEFBT	ELM_CB_PC_MAINT,	2
	DEFBT	ELM_CB_PCM_CNTRL_LSB,	0
	.equ	ELM_CB_PCM_START,	1*ELM_CB_PCM_CNTRL_LSB
	.equ	ELM_CB_PCM_TRACE,	2*ELM_CB_PCM_CNTRL_LSB
	.equ	ELM_CB_PCM_STOP,	3*ELM_CB_PCM_CNTRL_LSB

	.set	ELM_CB_PCM_OFF, ELM_CB_PCM_STOP

	.set	ELM_CB_LCT, ELM_CB_PC_LOOP | ELM_CB_LONG

	.set	m, ELM_CB_PC_MAINT
	.set	m,m| ELM_CB_PCM_STOP
	.equ	ELM_CB_M_DEFAULT, m


    ;ELM status register A, ELM*_STATUS_A
	DEFBT	ELM_SA_REV_LSB,		11
	.equ	ELM_SA_REV_MASK,	0x1f
	.equ	ELM_SA_REV_B,		0x00	;rev B
	.equ	ELM_SA_REV_D,		0x11	;rev D
	DEFBT	ELM_SA_SD,		10
	DEFBT	ELM_PREV_LS_LSB,	8
	.equ	ELM_PREV_LS_MASK,	3*ELM_PREV_LS_LSB
	.equ	ELM_PREV_LS_QLS,	0*ELM_PREV_LS_LSB
	.equ	ELM_PREV_LS_MLS,	1*ELM_PREV_LS_LSB
	.equ	ELM_PREV_LS_HLS,	2*ELM_PREV_LS_LSB
	.equ	ELM_PREV_LS_ILS,	3*ELM_PREV_LS_LSB
	DEFBT	ELM_LS_LSB,		5
	.equ	ELM_LS_MASK,		7*ELM_LS_LSB
	.equ	ELM_LS_NLS,		0*ELM_LS_LSB
	.equ	ELM_LS_ALS,		1*ELM_LS_LSB
	.equ	ELM_LS_ILS4,		3*ELM_LS_LSB
	.equ	ELM_LS_QLS,		4*ELM_LS_LSB
	.equ	ELM_LS_MLS,		5*ELM_LS_LSB
	.equ	ELM_LS_HLS,		6*ELM_LS_LSB
	.equ	ELM_LS_ILS,		7*ELM_LS_LSB
	DEFBT	ELM_LSM_STATE_LSB,	4
	DEFBT	ELM_U_LS,		3


    ;ELM status register B, ELM*_STATUS_B
	DEFBT	ELM_SB_RF_STATE,	14
	DEFBT	ELM_SB_PCI_STATE,	12
	DEFBT	ELM_SB_PCI_SCRUB,	11
	DEFBT	ELM_SB_PCM_STATE,	7
	DEFBT	ELM_SB_PCM_SIG,		6
	DEFBT	ELM_SB_LSF,		5
	DEFBT	ELM_SB_RCF,		4
	DEFBT	ELM_SB_TCF,		3
	DEFBT	ELM_SB_BREAK,		0
	DEFBT	ELM_SB_BREAK_MASK,	7*ELM_SB_BREAK
	DEFBT	ELM_SB_BREAK_PC_START,	1*ELM_SB_BREAK


    ;ELM event register, ELM*_EVENT
	DEFBT	ELM_EV_NP_ERR,		15
	DEFBT	ELM_EV_LSD,		14
	DEFBT	ELM_EV_LE_CTR,		13
	DEFBT	ELM_EV_MINI_CTR,	12
	DEFBT	ELM_EV_VSYM_CTR,	11
	DEFBT	ELM_EV_PHYINV,		10
	DEFBT	ELM_EV_EBUF,		9
	DEFBT	ELM_EV_TNE,		8
	DEFBT	ELM_EV_TPC,		7
	DEFBT	ELM_EV_PCM_ENABLED,	6
	DEFBT	ELM_EV_PCM_BREAK,	5
	DEFBT	ELM_EV_PCM_ST,		4
	DEFBT	ELM_EV_PCM_TRACE,	3
	DEFBT	ELM_EV_PCM_CODE,	2
	DEFBT	ELM_EV_LS,		1
	DEFBT	ELM_EV_PARITY_ERR,	0

    ;events that matter only when using ELM PCM
	.set	m,   ELM_EV_TPC
	.set	m,m| ELM_EV_PCM_ENABLED
	.set	m,m| ELM_EV_PCM_BREAK
	.set	m,m| ELM_EV_PCM_ST
	.set	m,m| ELM_EV_PCM_TRACE
	.set	m,m| ELM_EV_PCM_CODE
	.equ	ELM_EV_PCMS, m

    ;event mask during host PCM signaling
	.set	m,   ELM_EV_NP_ERR
	.set	m,m| ELM_EV_LS
	.equ	ELM_EV_SIG, m | ELM_EV_PCMS

    ;event mask while active
	.set	m,   ELM_EV_LE_CTR
	.set	m,m| ELM_EV_EBUF
	.set	m,m| ELM_EV_TNE
	.equ	ELM_EV_ACT, m | ELM_EV_SIG


    ;ELM Cipher Control Register, ELM*_CIPHER
	.equ	ELM_CIP_PRO_TEST,	14
	.equ	ELM_CPI_SDNRZEN,	8
	.equ	ELM_CPI_SDONEN,		7
	.equ	ELM_CPI_SDOFFEN,	6
	.equ	ELM_CPI_RXDATA_ENABLE,	5
	.equ	ELM_CPI_FOTOFF_SOURCE,	4
	.equ	ELM_CPI_FOTOFF_CONTROL,	2
	.equ	ELM_CPI_CIPHER_LBACK,	1
	.equ	ELM_CPI_CIPHER_ENABLE,	0


;MAC registers
;   Each is 16 bits, and so expect to use HWORD loads and stores
	.equ	MAC_CTRL_A,	0x00500102
	.equ	MAC_CTRL_B,	0x00500106
	.equ	MAC_MASK_A,	0x0050010a
	.equ	MAC_MASK_B,	0x0050010e
	.equ	MAC_MASK_C,	0x00500112
	.equ	MAC_MSA,	0x00500142
	.equ	MAC_MLA_A,	0x00500146
	.equ	MAC_MLA_B,	0x0050014a
	.equ	MAC_MLA_C,	0x0050014e
	.equ	MAC_T_REQ,	0x00500152
	.equ	MAC_TVX_T_MAX,	0x00500156
	.equ	MAC_EVNT_C,	0x00500176
	.equ	MAC_VOID_TIME,	0x0050017a
	.equ	MAC_TOKEN_CT,	0x0050017e
	.equ	MAC_FRAME_CT,	0x00500182
	.equ	MAC_LOST_ERR,	0x00500186
	.equ	MAC_EVNT_A,	0x0050018a
	.equ	MAC_EVNT_B,	0x0050018e
	.equ	MAC_RX_STATUS,	0x00500192
	.equ	MAC_TX_STATUS,	0x00500196
	.equ	MAC_T_NEG_A,	0x0050019a
	.equ	MAC_T_NEG_B,	0x0050019e
	.equ	MAC_INFO_A,	0x005001a2
	.equ	MAC_INFO_B,	0x005001a6
	.equ	MAC_BIST_SIG,	0x005001aa
	.equ	MAC_TVX_TIMER,	0x005001ae
	.equ	MAC_TRT_TIMER_A,0x005001b2
	.equ	MAC_TRT_TIMER_B,0x005001b6
	.equ	MAC_THT_TIMER_A,0x005001ba
	.equ	MAC_THT_TIMER_B,0x005001be
	.equ	MAC_PKT_REQ,	0x005001c2
	.equ	MAC_RC_CRC_A,	0x005001c6
	.equ	MAC_RC_CRC_B,	0x005001ca
	.equ	MAC_TX_CRC_A,	0x005001ce
	.equ	MAC_TX_CRC_B,	0x005001d2

    ;bits in MAC_CTRL_A
	DEFBT	MAC_CA_ON,	    15
	DEFBT	MAC_CA_REV_ADDR,    12
	DEFBT	MAC_CA_FLSH_SA47,   11
	DEFBT	MAC_CA_COPY_ALL2,   10
	DEFBT	MAC_CA_COPY_ALL1,   9
	DEFBT	MAC_CA_COPY_OWN,    8
	DEFBT	MAC_CA_COPY_ALLSMT, 7
	DEFBT	MAC_CA_COPY_SMT,    6
	DEFBT	MAC_CA_COPY_LLC,    5	    ;promiscuous
	DEFBT	MAC_CA_COPY_GRP,    4	    ;all-multicast
	DEFBT	MAC_CA_DSABL_BRDCST,3
	DEFBT	MAC_CA_RUN_BIST,    2

	.set	m,   MAC_CA_ON
	.set	m,m| MAC_CA_FLSH_SA47
	.set	m,m| MAC_CA_COPY_SMT
	.equ	MAC_CA_DEFAULT, m

	.set	m,   MAC_CA_DEFAULT
	.set	m,m| MAC_CA_DSABL_BRDCST
	.equ	MAC_CA_OFF, m

	.set	m,   MAC_CA_DEFAULT
	.set	m,m| MAC_CA_COPY_ALL2
	.set	m,m| MAC_CA_COPY_ALLSMT
	.set	m,m| MAC_CA_COPY_LLC
	.set	m,m| MAC_CA_COPY_GRP
	.equ	MAC_CA_PROM, m


    ;bits in MAC_CTRL_B
	DEFBT	MAC_CB_RING_PURGE,  15
	DEFBT	MAC_CB_FDX,	    14
	DEFBT	MAC_CB_B_STRIP,	    13
	DEFBT	MAC_CB_TXPARITY,    12
	DEFBT	MAC_CB_REPEAT,	    11
	DEFBT	MAC_CB_LOSE_CLAIM,  10
	DEFBT	MAC_CB_RESET,	    8
	DEFBT	MAC_CB_RESET_BEACON,9
	.equ    MAC_CB_RESET_CLAIM,   MAC_CB_RESET | MAC_CB_RESET_BEACON
	DEFBT	MAC_CB_FSI_BEACON,  7
	DEFBT	MAC_CB_DELAY_TOK,   6
	DEFBT	MAC_CB_NO_SCAM,	    5

	.equ	MAC_CB_DEFAULT, MAC_CB_TXPARITY | MAC_CB_NO_SCAM

	.set	m,   MAC_CB_LOSE_CLAIM
	.set	m,m| MAC_CB_REPEAT
	.set	m,m| MAC_CB_RESET_CLAIM
	.equ	MAC_CB_OFF, m
	.equ	MAC_CB_OFF_BT,	MAC_CB_LOSE_CLAIM_BT

    ;bits in MAC_EVNT_A
	DEFBT	MAC_EVA_PH_INVAL,   15
	DEFBT	MAC_EVA_UTOK_RCVD,  14
	DEFBT	MAC_EVA_RTOK_RCVD,  13
	DEFBT	MAC_EVA_TKN_CAP,    12
	DEFBT	MAC_EVA_BEACON_RCVD,11
	DEFBT	MAC_EVA_CLAIM_RCVD, 10
	DEFBT	MAC_EVA_FRAME_ERR,  9
	DEFBT	MAC_EVA_FRAME_RCVD, 8
	DEFBT	MAC_EVA_DBL_OVFL,   7
	DEFBT	MAC_EVA_RNGOP_CHG,  6
	DEFBT	MAC_EVA_BAD_T_OPR,  5
	DEFBT	MAC_EVA_TVX_EXPIR,  4
	DEFBT	MAC_EVA_LATE_TKN,   3
	DEFBT	MAC_EVA_RCVRY_FAIL, 2
	DEFBT	MAC_EVA_DUPL_TKN,   1
	DEFBT	MAC_EVA_DUPL_ADDR,  0

	.set	m,   MAC_EVA_FRAME_ERR
	.set	m,m| MAC_EVA_FRAME_RCVD
	.set	m,m| MAC_EVA_DBL_OVFL
	.set	m,m| MAC_EVA_RNGOP_CHG
	.set	m,m| MAC_EVA_TVX_EXPIR
	.set	m,m| MAC_EVA_LATE_TKN
	.set	m,m| MAC_EVA_RCVRY_FAIL
	.set	m,m| MAC_EVA_DUPL_TKN
	.set	m,m| MAC_EVA_DUPL_ADDR
	.equ	MAC_MASK_A_DEFAULT, m

    ;bits in MAC_EVNT_B
	DEFBT	MAC_EVB_MY_BEACON,  15
	DEFBT	MAC_EVB_OTR_BEACON, 14
	DEFBT	MAC_EVB_HI_CLAIM,   13
	DEFBT	MAC_EVB_LO_CLAIM,   12
	DEFBT	MAC_EVB_MY_CLAIM,   11
	DEFBT	MAC_EVB_BAD_BID,    10
	DEFBT	MAC_EVB_PURGE_ERR,  9
	DEFBT	MAC_EVB_BS_ERR,	    8
	DEFBT	MAC_EVB_WON_CLAIM,  7
	DEFBT	MAC_EVB_NP_ERR,	    6
	DEFBT	MAC_EVB_SI_ERR,	    5
	DEFBT	MAC_EVB_NOT_COPIED, 4
	DEFBT	MAC_EVB_FDX_CHG,    3
	DEFBT	MAC_EVB_BIT4_I_SS,  2
	DEFBT	MAC_EVB_BIT5_I_SS,  1
	DEFBT	MAC_EVB_BAD_CRC_TX, 0

	.set	m,   MAC_EVB_NP_ERR
	.set	m,m| MAC_EVB_SI_ERR
	.set	m,m| MAC_EVB_FDX_CHG
	.equ	MAC_MASK_B_BUG, m

	.set	m, MAC_MASK_B_BUG
	.set	m,m| MAC_EVB_BAD_BID
	.set	m,m| MAC_EVB_PURGE_ERR
	.set	m,m| MAC_EVB_WON_CLAIM
	.set	m,m| MAC_EVB_NOT_COPIED
	.equ	MAC_MASK_B_DEFAULT, m

	.set	m,   MAC_EVB_MY_BEACON
	.set	m,m| MAC_EVB_OTR_BEACON
	.set	m,m| MAC_EVB_HI_CLAIM
	.set	m,m| MAC_EVB_LO_CLAIM
	.set	m,m| MAC_EVB_MY_CLAIM
	.set	MAC_MASK_B_MORE, m

    ;bits in MAC_EVNT_C
	DEFBT	MAC_EVC_VOID_RDY,   2
	DEFBT	MAC_EVC_VOID_OVF,   1
	DEFBT	MAC_EVC_TKN_CNT_OVF,0

	.set	m,   MAC_EVC_VOID_RDY
	.set	m,m| MAC_EVC_VOID_OVF
	.set	m,m| MAC_EVC_TKN_CNT_OVF
	.equ	MAC_MASK_C_DEFAULT, m

    ;bits in MAC_TX_STATUS
	DEFBT	MAC_TX_ST_FSM_MSB,  15
	DEFBT	MAC_TX_ST_FSM_LSB,  12
	.equ	MAC_TX_ST_BEACON,	0x6*MAC_TX_ST_FSM_LSB
	.equ	MAC_TX_ST_CLAIM,	0x7*MAC_TX_ST_FSM_LSB
	.equ	MAC_TX_ST_FSM_MASK,	0xf*MAC_TX_ST_FSM_LSB
	DEFBT	MAC_TX_ST_RNGOP,    11
	DEFBT	MAC_TX_ST_PURGING,  10

    ;bits in MAC_PKT_REQ
	DEFBT	MAC_PRH_BCN,	    14
	DEFBT	MAC_PRH_USER_R,	    13
	DEFBT	MAC_PRH_PREV_SEND_LAST,12
	DEFBT	MAC_PRH_TOK_TYP_MSB,11
	DEFBT	MAC_PRH_TOK_TYP_LSB,10
	 .equ	MAC_PRH_TOK_TYP_IMM,	0*MAC_PRH_TOK_TYP_LSB
	 .equ	MAC_PRH_TOK_TYP_RES,	1*MAC_PRH_TOK_TYP_LSB
	 .equ	MAC_PRH_TOK_TYP_UNR,	2*MAC_PRH_TOK_TYP_LSB
	 .equ	MAC_PRH_TOK_TYP_ANY,	3*MAC_PRH_TOK_TYP_LSB
	DEFBT	MAC_PRH_SYNCH,	    9
	DEFBT	MAC_PRH_IMM_MODE,   8
	DEFBT	MAC_PRH_SEND_FIRST, 7
	DEFBT	MAC_PRH_SEND_LAST,  6
	DEFBT	MAC_PRH_CRC,	    5
	DEFBT	MAC_PRH_TOKEN_SEND, 4

;frames are padded with 3 bytes by the MAC.
;   The host driver code knows about output padding as
	.equ	FRAME_PAD_LEN, 3

;   The 3 bytes in memory before the FC are called the "packet request header"

	DEFBT	PRH_CTOK_MSB,	    5+24
	DEFBT	PRH_CTOK_LSB,	    4+24
	 .equ	PRH_CTOK_IMM,		0*PRH_CTOK_LSB
	 .equ	PRH_CTOK_RES,		1*PRH_CTOK_LSB
	 .equ	PRH_CTOK_UNR,		2*PRH_CTOK_LSB
	 .equ	PRH_CTOK_ANY,		3*PRH_CTOK_LSB
	DEFBT	PRH_SYNCH,	    3+24
	DEFBT	PRH_IMM_MODE,	    2+24
	DEFBT	PRH_SEND_FIRST,	    1+24
	DEFBT	PRH_BCN,	    0+24
	DEFBT	PRH_SEND_LAST,	    6+16
	DEFBT	PRH_CRC,	    5+16
	DEFBT	PRH_STOK_LSB,	    3+16
	 .equ	PRH_STOK_NONE,		0*PRH_STOK_LSB
	 .equ	PRH_STOK_UNR,		1*PRH_STOK_LSB
	 .equ	PRH_STOK_RES,		2*PRH_STOK_LSB
	 .equ	PRH_STOK_SAME,		3*PRH_STOK_LSB

	.equ	FSIBASE, FSI_GSR
 .macro	STOFSI,	val,port,tmp0,tmp1,fs
   .if ! $ISREG(val)
	  constx  tmp1,val
	  OPST	  store,,FSIBASE,p_fsi,FSI_@port@,tmp1,tmp0,fast
   .else
	  OPST	  store,,FSIBASE,p_fsi,FSI_@port@,val,tmp0,fast
   .endif
 .endm

 .macro	LODFSI,	tgt,port,fs
	  OPST	  load,,FSIBASE,p_fsi,FSI_@port@,tgt,tgt,fast
 .endm


;Wait for the FSI control register to be free,
;   and leave the temporary register = 0x80000000
	CK	FSIBASE == FSI_GSR
	CK	GSR_CRF_BT > GSR_IOE_BT
 .macro WFSI_CRF, tmp
	  load	  0,WORD,tmp,p_fsi
	  tbit	  tmp,tmp,GSR_CRF
	  jmpt	  tmp,.+3*4
	  sll	  tmp,tmp,31-(GSR_IOE_BT-GSR_CRF_BT)
	  jmpf	  tmp,.-4*4
	  cplt	  tmp,tmp,0
 .endm

;wait for the FSI to be ready for another command
;   assume the caller worries about the branch delay slot
	CK	FSIBASE == FSI_GSR
	CK	GSR_CDN_BT == 31
 .macro	WFSI_CDN, tmp
	  load	  0,WORD,tmp,p_fsi
	  jmpt	  tmp,.+3*4
	  tbit	  tmp,tmp,GSR_IOE
	  jmpf	  tmp,.-3*4
 .endm

;This macro is known to generate one instruction and is used
;   in branch delay slots.
 .macro	C_IRI,	reg, tgt,iri,dat
 .if (((FSI_@tgt@) | (iri)*FSI_IRI | (dat)*FSI_DATA) & 0xffff) != 0
	.print	" ** ERROR: non-zero least significant bytes"
	.err
   .endif
	  consth reg,(FSI_@tgt@) | (iri)*FSI_IRI | (dat)*FSI_DATA
 .endm

;command the FSI
;interrupts should be off
	CK	FSIBASE != FSI_CER	;would otherwise need a nop
 .macro	CMD_FSI, cmr,cer,tmp1,tmp2,fs
	  WFSI_CDN tmp1
	  STOFSI  cer,CER,tmp1,tmp2,fs
	  STOFSI  cmr,CMR,tmp1,tmp2,fs
 .endm

;reset a ring
	CK	FSIBASE != FSI_CMR	;would otherwise need a nop
 .macro	RRESET,	r,tmp1,tmp2,fs
	  WFSI_CDN tmp1
	  STOFSI FSI_CMD_RRESET|((r)*FSI_CMD_RING_N),CMR,tmp1,tmp2,fs
 .endm

;stop a ring
	CK	FSIBASE != FSI_CMR	;would otherwise need a nop
 .macro	RSTOP,	r,tmp1,tmp2,fs
	  WFSI_CDN tmp1
	  STOFSI FSI_CMD_RSTOP|((r)*FSI_CMD_RING_N),CMR,tmp1,tmp2,fs
 .endm

;initialize an internal FSI register
;interrupts should be off
 .macro	STO_IR,	tgt,iri,dat,tmp,fs
  .ifeqs "@fs@","fast"
  .else
   .ifnes "@fs@",""
	.print "** ERROR: @fs@ is not 'fast' or null"
	.err
   .endif
	  WFSI_CRF tmp
  .endif
	  C_IRI  tmp, tgt,iri,dat
	  store   0,WORD,tmp,p_fsifcr
 .endm


;fetch an internal FSI register
;interrupts should be off
 .macro	LOD_IR,	reg,tgt,iri,fs
	  WFSI_CRF reg
   .if $ISREG(tgt)
	  store   0,WORD,tgt,p_fsifcr
   .else
	  C_IRI reg,tgt|FSI_R,iri,0
	  store   0,WORD,reg,p_fsifcr
   .endif
	  WFSI_CRF reg
	  load	  0,WORD,reg,p_fsifcr
 .endm


;set a MAC register
	.equ	MACBASE, MAC_CTRL_A
 .macro	STOMAC,	val,port,tmp,fs
	  OPST	  store,H,MACBASE,p_mac,MAC_@port@,val,tmp,fs
 .endm
 .macro	LODMAC,	tgt,port,fs
	  OPST	  load,H,MACBASE,p_mac,MAC_@port@,tgt,tgt,fs
 .endm

	.equ	ELMBASE, ELM0_STATUS_A+0x80
 .macro	STOELM0, val,port,tmp,fs
	  OPST	  store,H,ELMBASE,p_elm,ELM0_@port@,val,tmp,fs
 .endm
 .macro	LODELM0, tgt,port,fs
	  OPST	  load,H,ELMBASE,p_elm,ELM0_@port@,tgt,tgt,fs
 .endm

 .macro	STOELM1, val,port,tmp,fs
	  OPST	  store,H,ELMBASE,p_elm,ELM1_@port@,val,tmp,fs
 .endm
 .macro	LODELM1, tgt,port,fs
	  OPST	  load,H,ELMBASE,p_elm,ELM1_@port@,tgt,tgt,fs
 .endm

;clear previos ELM PCM command
 .macro	CLRELM0, tgt,tmp
	  mtsrim  CPS,PS_INTSOFF
	  LODELM0 tgt, CTRL_B,fast
	  constx  tmp,ELM_CB_MATCH_LS_MASK
	  and	  tgt,tgt,tmp
	  STOELM0 tgt, CTRL_B,tmp,fast
 .endm
 .macro	CLRELM1, tgt,tmp
	  mtsrim  CPS,PS_INTSOFF
	  LODELM1 tgt, CTRL_B,fast
	  constx  tmp,ELM_CB_MATCH_LS_MASK
	  and	  tgt,tgt,tmp
	  STOELM1 tgt, CTRL_B,tmp,fast
 .endm

;send a PCM command to an ELM
 .macro	CMDELM0, cmd,tgt,tadr,tmsk
	  sub	  tadr,p_elm,ELMBASE-ELM0_CTRL_B
	  CMDELMX cmd,tgt,tadr,tmsk
 .endm
 .macro	CMDELM1, cmd,tgt,tadr,tmsk
	  add	  tadr,p_elm,ELM1_CTRL_B-ELMBASE
	  CMDELMX cmd,tgt,tadr,tmsk
 .endm
 .macro	CMDELMX, cmd,tgt,tadr,tmsk
	  mtsrim  CPS,PS_INTSOFF
	  load	  0,HWORD,tgt,tadr
	  constx  tmsk,ELM_CB_MATCH_LS_MASK
	  and	  tgt,tgt,tmsk
	  store   0,HWORD,tgt,tadr
	  or	  tgt,tgt,ELM_CB_@cmd@
	  mtsrim  MMU,0xff
	  store   0,HWORD,tgt,tadr
 .endm


;declare a small buffer that can be DMA'ed without worrying about
;   the 512 byte restriction in the DMA hardware.
 .macro DMABLK,	lab,len,maxpad
  .if len > DMA_ALIGN
	.print	" ** ERROR: @len@ is > DMA_ALIGN"
	.err
  .endif
	BALIGN	DMA_ALIGN,len,maxpad,
  .ifnes "@lab@",""
lab:	  .block      len
  .else
	  .block      len
  .endif
 .endm

;align to something
 .macro	BALIGN,	bnd,len,maxpad
  .if (. & -bnd) != ((.+(len)-1) & -bnd)
	.set	THIS_BALIGN, ((0-.) & (bnd-1))
    .if THIS_BALIGN > maxpad+64
	  .set	toobig, THIS_BALIGN
	  .print " ** ERROR: wasteful padding"
	  .err
    .endif
	  .block  THIS_BALIGN
	  .set	  TOT_BALIGN, TOT_BALIGN+THIS_BALIGN
  .endif
 .endm
	.set	TOT_BALIGN, 0


;test to see if the DMA machinery is active
;   reg=bit 31=1 (TRUE) if DMA is quiet,  other bits indeterminate
 .macro	TDMA,	reg
	  sub	  reg,reg,BLUR_C_LSB
	  tbit	  reg,reg,BLUR_C_MSB
 .endm

 .macro	CKDMA,	reg
	  load	  0,WORD,reg,p_blur
	  TDMA	reg
 .endm


;wait for the most recent DMA operation to finish
;   use temporary register tmp
 .macro	WAITDMA,tmp,fs
	CKDMA	tmp
	  jmpf	  tmp,.-3*4
  .ifeqs "@fs@","fast"
  .else
   .ifnes "@fs@",""
	.print "** ERROR: @fs@ is not 'fast' or null"
	.err
   .endif
	   nop
  .endif
 .endm


 .ifdef MEZ
;do a dummy host-to-board DMA operation to flush the "cache" in the DANG
;   Any previous DMA operation must be known to be finished.
;   Its first instruction is safe for a delay slot.
 .macro	DUMDMA,tmp
	DOLDMA	4,H2B,,dma_dummy,host_cmd_buf,,tmp
 .endm
 .endif


;start a lo-core DMA operation--normally use STLDMA
;   This only works on the first or low 4GB of host RAM.
;   Use temporary register tmp, byte count cnt, SRAM address regloc+offloc
;   and system address reghost+offhost
;   Its first instruction is safe for a delay slot.
 .macro DOLDMA,	cnt,b2h,regloc,offloc,reghost,offhost,tmp
  .ifdef DANG_BUG
    .ifeqs "@b2h@","H2B"
	rs_st	DANG_DMA
    .else
	t_st	tmp,DANG_DMA
	  jmpf	  tmp,$1
	   DUMDMA tmp
	WAITDMA	tmp,fast
$1:	  set_st DANG_DMA
    .endif
  .endif
  .if $ISREG(cnt)
	  consth  tmp,CREG_B2H
	  store	  0,WORD,cnt,p_blur
  .else
	  const	  tmp,cnt
	  store	  0,WORD,tmp,p_blur
	  consth  tmp,CREG_B2H
  .endif
  .ifeqs "@b2h@","B2H"
	  or	  cregs,cregs,tmp
  .else
	  andn	  cregs,cregs,tmp
     .ifnes "@b2h@","H2B"
	.print	"** ERROR: @b2h@ is not B2H or H2B"
	.err
     .endif
  .endif
	  store	  0,WORD,cregs,p_creg
  .ifeqs "@regloc@",""
	  constx  tmp,offloc
	  store	  0,WORD,tmp,p_crap
  .else
    .if offloc+0 != 0
	  add	  tmp,regloc,offloc
	  store	  0,WORD,tmp,p_crap
     .else
	  store	  0,WORD,regloc,p_crap
    .endif
  .endif
  .ifeqs "@reghost@",""
	  constx  tmp,offhost
	  store	  0,WORD,tmp,p_vsar
  .else
    .if offhost+0 != 0
	  add	  tmp,reghost,offhost
	  store	  0,WORD,tmp,p_vsar
     .else
	  store	  0,WORD,reghost,p_vsar
    .endif
  .endif
  .ifdef MEZ
    .ifeqs "@b2h@","H2B"
	rs_st	DANG_CACHE
    .endif
  .endif
 .endm


;Start a low core DMA operation when it is safe.
;   This only works on the first or low 4GB of host RAM.
 .macro STLDMA,	cnt,b2h,regloc,offloc,reghost,offhost,tmp
	WAITDMA	tmp,fast,
	DOLDMA	cnt,b2h,regloc,offloc,reghost,offhost,tmp,
 .endm
