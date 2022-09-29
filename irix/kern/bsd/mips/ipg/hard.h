; Interphase 4211 Peregrine hardware definitions
;
; Copyright 1990 Silicon Graphics, Inc.  All rights reserved.
;	"$Revision: 1.18 $"


	.equ	MHZ, 16			;29K clock speed, in MHz
	.equ	REV29K,3		;assume Rev-D 29K

	.equ	PFETCH, 8		;max cycles of prefetch 
	.equ	LD_WAIT, 3		;cycles consumed by a LOAD
	.equ	ST_WAIT, 5		;cycles consumed by a STORE

 .macro	DEFWAIT,nm,usec
	  .equ	  nm, ((usec)*MHZ) / 1000 + LD_WAIT-1
 .endm

;well known traps
	.equ	V_FOR_CMT, V_TRAP0
	.equ	V_RX,	V_TRAP1
	.equ	V_TX_S,	V_INTR0
	.equ	V_TX_A,	V_INTR1
	.equ	V_BUS,	V_INTR2
	.equ	V_MB,	V_INTR3


;The 4211 memory map
	.equ	SRAM_BASE,  0
	.equ	SRAM_SIZE,  128*1024

	.equ	FASTIO_BASE,0x40000000

	.equ	DMACS_BASE, FASTIO_BASE + 0x14000000
	.equ	DMACS_CH1,  	DMACS_BASE+ 0x00	;TXS
	.equ	DMACS_CH2,	DMACS_BASE+ 0x20	;TXA
	.equ	DMACS_CH3,	DMACS_BASE+ 0x40
	.equ	DMACS_CH4,	DMACS_BASE+ 0x60
	.equ	IO_EXPORT,  FASTIO_BASE + 0x18000000	;"export" reg	(w)
	.equ	IO_IMPORT,  FASTIO_BASE + 0x18800002	;"import" reg	(r)
	.equ	IO_LDAM,    FASTIO_BASE + 0x1b800000	;set VME AM	(w)
	.equ	IO_LDVADDR, FASTIO_BASE + 0x1c000000	;VME addr	(w)
	.equ	IO_LDVME,   FASTIO_BASE + 0x1c800000	;VME count	(w)
	.equ	IO_CLRMBINT,FASTIO_BASE + 0x1d000000	;clear mailbox int (r)
	.equ	IO_REQBUS,  FASTIO_BASE + 0x1d800000	;req. VME bus	(r)
	.equ	IO_SVINT,   FASTIO_BASE + 0x1e000000	;interrupt host	(r)
	.equ	IO_BUSRST,  FASTIO_BASE + 0x1e800000	;clear VME intr	(r)
	.equ	IO_STRT01,  FASTIO_BASE + 0x1f000000	;VME word 01	(r)
	.equ	IO_STRT23,  FASTIO_BASE + 0x1f800000	;VME word 23	(r)

	.equ	SLOWIO_BASE,0x60000000
	.equ	IO_FORMAC,  SLOWIO_BASE + 0x00000002
	.equ	IO_FORMAC_RESET,IO_FORMAC + 0x00    ;reset		(w)
	.equ	IO_FORMAC_TLSB,	IO_FORMAC_RESET	    ;read LSBs		(r)
	.equ	IO_FORMAC_IDLS,	IO_FORMAC + 0x04    ;force Idle state	(w)
	.equ	IO_FORMAC_TNEG,	IO_FORMAC_IDLS	    ;TNeg		(r)
	.equ	IO_FORMAC_CLLS,	IO_FORMAC + 0x08    ;force Claim state	(w)
	.equ	IO_FORMAC_BCLS,	IO_FORMAC + 0x0c    ;force Beacon state	(w)
	.equ	IO_FORMAC_THT,	IO_FORMAC + 0x10    ;THT
	.equ	IO_FORMAC_TRT,	IO_FORMAC + 0x14    ;TRT
	.equ	IO_FORMAC_TMAX,	IO_FORMAC + 0x18    ;TMAX
	.equ	IO_FORMAC_RCTL,	IO_FORMAC + 0x1c    ;control Restricted Token
	.equ	IO_FORMAC_TVX,	IO_FORMAC + 0x20    ;TVX
	.equ	IO_FORMAC_SNDM,	IO_FORMAC + 0x24    ;send immediate	(W)
	.equ	IO_FORMAC_STMC,	IO_FORMAC_SNDM	    ;state machine	(R)
	.equ	IO_FORMAC_MODE,	IO_FORMAC + 0x28    ;mode register	(R/W)
	.equ	IO_FORMAC_IMSK,	IO_FORMAC + 0x2c    ;interrupt mask	(R/W)
	.equ	IO_FORMAC_CLAPTR,IO_FORMAC+ 0x30    ;clear address pointer (W)
	.equ	IO_FORMAC_STAT,	IO_FORMAC_CLAPTR    ;status register	(R)
	.equ	IO_FORMAC_ADRS,	IO_FORMAC + 0x34    ;address register
	.equ	IO_FORMAC_TPRI,	IO_FORMAC + 0x38    ;TPRI		(R/W)

	.equ	IO_ENDEC1,  SLOWIO_BASE + 0x04000002
	.equ	IO_ENDEC1_REG,	IO_ENDEC1 + 0x00    ;data register
	.equ	IO_ENDEC1_SEL,	IO_ENDEC1 + 0x04    ;select register

	.equ	IO_CMD1,    SLOWIO_BASE + 0x08000002;Front End command/status
	.equ	IO_TXS_ACK,	IO_CMD1 + 0x04	    ;acknowledge interrupts
	.equ	IO_TXA_ACK,	IO_CMD1 + 0x08
	.equ	IO_RX3_ACK,	IO_CMD1 + 0x10
	.equ	IO_RX4_ACK,	IO_CMD1 + 0x20

	.equ	IO_9513_1,  SLOWIO_BASE + 0x0c000002
	.equ	IO_CNT1_DATA,	IO_9513_1 + 0
	.equ	IO_CNT1_CMD,	IO_9513_1 + 4

	.equ	IO_CMT1,    SLOWIO_BASE + 0x10000003

	.equ	IO_ENDEC2, SLOWIO_BASE + 0x19800002
	.equ	IO_ENDEC2_REG,	IO_ENDEC2 + 0x00    ;data register
	.equ	IO_ENDEC2_SEL,	IO_ENDEC2 + 0x04    ;select register

	.equ	IO_9513_2,  SLOWIO_BASE + 0x19800012
	.equ	IO_CNT2_DATA,	IO_9513_2 + 0
	.equ	IO_CNT2_CMD,	IO_9513_2 + 4

	.equ	IO_CMT2,    SLOWIO_BASE + 0x19800023

	.equ	IO_CMD2,    SLOWIO_BASE + 0x19800032

	.equ	IO_CAM,	    SLOWIO_BASE + 0x1a000000
	.equ	IO_ASYNC,   SLOWIO_BASE + 0x1b000000

	.equ	DRAM_BASE,  0x80000000
	.equ	VME_VEC,    DRAM_BASE		;interrupt vector
	.equ	VME_VEC_SIZE,16
	.equ	VME_VEC_END,VME_VEC+VME_VEC_SIZE
	.equ	DRAM_SIZE,  1024*1024

	.equ	FE_ALIGN, 16		;align buffers to at least this
	.equ	FE_AMSK, FE_ALIGN-1

	.equ	SIO_BASE,   DRAM_BASE+0xff000
	.equ	SIO_INCR,   0x10
	.equ	SIO_SIZE,   256*16	;256 half-words or 512 bytes

	.equ	XFER_CYCLE, 0x80100000
	.equ	VRAM_RESET, 0x80302814

	.equ	DMA1_ADR,   0x80800000	;TXS
	.equ	DMA2_ADR,   0x80a00000	;TXA
	.equ	DMA3_ADR,   0x80c00000
	.equ	DMA4_ADR,   0x80e00000
	.equ	DMA_ADR_MSK,0x000ffff0



;the length of EPROM code in the first byte of the EPROM is divided by this.
	DEFBT	EPROM_SIZE, 12
	.equ	EPROM_BASE, 0xc0000000


;bits in the "export" register
	DEFBT	IO_EXPORT_NVD, 15	;NVRAM write data
	DEFBT	IO_EXPORT_NVC, 14	;NVRAM command clock
	DEFBT	IO_EXPORT_NVE, 13	;NVRAM enable
	DEFBT	IO_EXPORT_LED, 12	;LED, 1=green, 0=red
	DEFBT	IO_EXPORT_SFL, 11	;0=VME SYSFAIL
	DEFBT	IO_EXPORT_EBR, 10	;1=enable VME DMA bursting
	DEFBT	IO_EXPORT_BWR, 9	;1=VME bus write
	DEFBT	IO_EXPORT_32B, 8	;1=VME long words
	DEFBT	IO_EXPORT_SEQ, 7	;0=VME block mode transfers
	DEFBT	IO_EXPORT_DBP, 6	;0=disable VME address bump
	DEFBT	IO_EXPORT_SET, 5	;0=set VME A1
	DEFBT	IO_EXPORT_DS1, 4	;VME data strobe 1
	DEFBT	IO_EXPORT_DS0, 3	;VME data strobe 0
	DEFBT	IO_EXPORT_IP2, 2	;VME BRL
	DEFBT	IO_EXPORT_IP1, 1
	DEFBT	IO_EXPORT_IP0, 0

	.equ	IO_EXPORT_IP, IO_EXPORT_IP2 | IO_EXPORT_IP1 | IO_EXPORT_IP0
				    ;as29 cannot do long lines
	.equ	IO_EXPORT_STD1,(IO_EXPORT_SFL | IO_EXPORT_EBR | IO_EXPORT_BWR)
	.equ	IO_EXPORT_STD2,(IO_EXPORT_32B | IO_EXPORT_DBP)
	.equ	IO_EXPORT_STD3,(IO_EXPORT_DS1|IO_EXPORT_DS0)
	.equ	IO_EXPORT_STD, IO_EXPORT_STD1|IO_EXPORT_STD2|IO_EXPORT_STD3


;bits in the "import" register
	DEFBT	IO_IMPORT_VBI, 5	;VME bus error interrupt
	DEFBT	IO_IMPORT_VDI, 4	;VME bus done interrupt
	DEFBT	IO_IMPORT_ABP, 3	;async board present
	DEFBT	IO_IMPORT_INT, 2	;host interrupts status
	DEFBT	IO_IMPORT_MBI, 1	;mailbox interrupt
	DEFBT	IO_IMPORT_NVD, 0	;NVRAM read data


;values for the BUSRST "strobe"
	.equ	BUSRST_READ,	0x30000
	.equ	BUSRST_WRITE,	0x00000

	DEFBT	BUS_PAGE, 11		;"bus packet page" size



;There is a problem on the board which causes the 2nd of two back-to-back
;   accesses of anything in the front end to not have enough wait states.
;   This restriction applies to the FORMAC, the ENDECs, the CMD1 and
;   CMD2 command and status registers, the 9513s, the CMT PALs, and the CAM.
;Interphase says 200 nsec of delay are required.

	.equ	FE_REC, 200
	.equ	FE_DELAY_CYCLE, (FE_REC*MHZ+999)/1000
	.equ	FE_RESET_REC, 160	;recovery after FE_ENABLE changes
 .macro	FE_DELAY, cycles, pf,
 .if	(FE_DELAY_CYCLE-(cycles+0)+(pf+PFETCH)) < 0x80000000
	  NDELAY  notused, FE_REC, cycles+0
 .endif
 .endm



;bits in the front end command register, IO_CMD1
	DEFBT	CMD1_FCMS_EXT,	15	;external 12-bit bus input
	DEFBT	CMD1_FCMS_END,	14	;ENDEC R bus
	DEFBT	CMD1_FCMS_FOR,	13	;FORMAC X bus
	DEFBT	CMD1_OBC1,	12	;enable optical bypass 1
	DEFBT	CMD1_LQME1,	11	;enable line quality monitor 1
	DEFBT	CMD1_FE_ENABLE,	10	;front end enable
	DEFBT	CMD1_RXALIGN,	9	;pad input with 3 leading bytes
	DEFBT	CMD1_DATTACH,	8	;connect bus to other board
	DEFBT	CMD1_OBC2,	4	;enable optical bypass 2
	DEFBT	CMD1_LQME2,	3	;enable line quality monitor 2
	DEFBT	CMD1_TXABT,	2	;transmit abort
	DEFBT	CMD1_MEDREQA,	1	;hog async. output
	DEFBT	CMD1_MEDREQS,	0	;hog sync. output

	.equ	CMD1_RESET, CMD1_TXABT  | CMD1_RXALIGN|CMD1_LQME1|CMD1_LQME2
	.equ	CMD1_GO, CMD1_RESET ^ (CMD1_FE_ENABLE | CMD1_TXABT)
	

;status bits
	DEFBT	STAT1_CMT1,	15	;1=CMT 1 interrupting
	DEFBT	STAT1_CMT2,	14	;1=CMT 2 interrupting
	DEFBT	STAT1_CNT1,	13	;first 9513
	DEFBT	STAT1_CNT2,	12	;second 9513
	DEFBT	STAT1_FORMAC,	11	;formac interrupt request
	DEFBT	STAT1_OBC,	10	;optical bypass present
	DEFBT	STAT1_MAC2,	9	;2nd board connected
	DEFBT	STAT1_PHY2,	8	;2nd ENDEC present
;XXX	DEFBT	STAT1_		7
	DEFBT	STAT1_TX_UNDER,	6	;transmit underrun
	DEFBT	STAT1_RX4_CONT,	5	;2nd channel RX continuation
	DEFBT	STAT1_RX3_CONT,	4	;1st channel RX continuation
	DEFBT	STAT1_RX4_INT,	3	;2nd RX channel interrupt
	DEFBT	STAT1_RX3_INT,	2	;1st RX channel interrupt
	DEFBT	STAT1_TXA_INT,	1	;async TX interrupt
	DEFBT	STAT1_TXS_INT,	0	;sync TX interrupt

;bits in the 2nd command register, IO_CMD2
	DEFBT	CMD2_CAM_SM_ENABLE,10
	DEFBT	CMD2_CAM_EN,	7
	DEFBT	CMD2_CAM_48ADR,	6
	DEFBT	CMD2_DAMAT,	5
	DEFBT	CMD2_SAMAT,	4
	DEFBT	CMD2_MCAST,	3

;status bits
	DEFBT	STAT2_MCAST,	1



;9513 values
	.equ	IOCNT_RESET,	0xffff  ;chip "master reset"
	.equ	IOCNT_DISABLE_FOUT,0xffee   ;turn off FOUT
	.equ	IOCNT_ENABLE_16BIT,0xffef   ;set Master Mode bit 13
	.equ	IOCNT_DISARM_ALL,0xffdf	;disarm all counters
	.equ	IOCNT_ARM_ALL,	0xff3f  ;arm all counters
	.equ	IOCNT_ARM_ONE,	0xff20	;arm one counter 
	.equ	IOCNT_SAVE_ALL,	0xffbf  ;save all counters
	.equ	IOCNT_LOAD_ALL,	0xff5f  ;load all counters
	.equ	IOCNT_MODE,	0xff00	;point to mode register
	.equ	IOCNT_HOLD,	0xff18	;point to load register
	.equ	IOCNT_CLR_TC,	0xffe0	;clear TC Toggle output

	.equ	IOCNT_F1_RES,	4	;F1 resolution, counts/usec
	.equ	IOCNT_OVF,	1<<16   ;counters overflow here

	.equ	IOCNT_OFF,	0	;disable 9513 output
	.equ	IOCNT_TOGGLE,	2	;"TC Toggled" ouptut


	DEFWAIT	WAIT_9513, 160		;cycles to access a counter
	.equ	RD_REC_9513, 1000	;nsec read recovery time
	.equ	WR_REC_9513, 1500	;nsec write recovery time




;FORMAC status and interrupt mask registers
	DEFBT	FORMAC_SRNGOP,	15	;ring is operational
	DEFBT	FORMAC_SMULTDA,	14	;multiple destination address
	DEFBT	FORMAC_SMISFRM,	13	;missed frame
	DEFBT	FORMAC_SXMTABT,	12	;xmit abort
	DEFBT	FORMAC_STKERR,	11	;token error
	DEFBT	FORMAC_SCLM,	10	;claiming
	DEFBT	FORMAC_SBEC,	9	;beaconing
	DEFBT	FORMAC_STVXEXP,	8	;TVX expired
	DEFBT	FORMAC_STRTEXP,	7	;TRT expired
	DEFBT	FORMAC_STKISS,	6	;token issued
	DEFBT	FORMAC_SADET,	5	;received DA matched
	DEFBT	FORMAC_SMYCLM,	4	;my claim has been received
	DEFBT	FORMAC_SLOCLM,	3	;lower claim frame received
	DEFBT	FORMAC_SHICLM,	2	;higher claim received
	DEFBT	FORMAC_SMYBEC,	1	;my beacon has been received
	DEFBT	FORMAC_SOTRBEC,	0	;some other received

;selected FORMAC state machine register bits
	DEFBT	FORMAC_XMIT5,	9	;beacon
	DEFBT	FORMAC_XMIT4,	8	;claim

	DEFWAIT	FORMAC_WAIT, 390	;cycles to access FORMAC
	.equ	FORMAC_REC_NSEC,FE_REC	;recovery time--really 120

;selected FORMAC mode bits
	.equ	FORMAC_MMODE_ONL,0x6000	;on line
	.equ	FORMAC_MMODE_OFF,0xe000	;off
	DEFBT	FORMAC_PROM, 9		;MADET1 for promiscuous


;delay to let the FORMAC recover
;   treg=scratch register, cycles=delay already available
 .macro	FORMAC_REC, cycles, pf,
 .if	(((FORMAC_REC_NSEC)*MHZ+999)/1000-(cycles+0)+(pf+PFETCH)) < 0x80000000
	  NDELAY  notused, FORMAC_REC_NSEC, cycles+0
 .endif
 .endm


;pick out the bits of an ENDEC command
	.equ	ENDEC_MASK, 0x0300
	.equ	ENDEC_SLL,  8

;ENDEC control register addresses
	.equ	ENDEC_SEL_CR0,	    0x0000
	.equ	ENDEC_SEL_CR1,	    0x0100
	.equ	ENDEC_SEL_CR2,	    0x0200
	.equ	ENDEC_SEL_STATUS,   0x0300
	.equ	ENDEC_SEL_CR3,	    0x0400

;ENDEC CR0 modes
	.equ	ENDEC_CR0_QLS,	    0x00+ENDEC_SEL_CR0
	.equ	ENDEC_CR0_MLS,	    0x01+ENDEC_SEL_CR0
	.equ	ENDEC_CR0_HLS,	    0x02+ENDEC_SEL_CR0
	.equ	ENDEC_CR0_ILS,	    0x03+ENDEC_SEL_CR0
	.equ	ENDEC_CR0_FILTER,   0x04+ENDEC_SEL_CR0
	.equ	ENDEC_CR0_JKILS,    0x05+ENDEC_SEL_CR0
	.equ	ENDEC_CR0_IQ,	    0x06+ENDEC_SEL_CR0
	.equ	ENDEC_CR0_UNFILTER, 0x07+ENDEC_SEL_CR0

;ENDEC CR1 modes
	.equ	ENDEC_CR1_TA,	    0x04+ENDEC_SEL_CR1	;TA-bus
	.equ	ENDEC_CR1_TB,	    0x00+ENDEC_SEL_CR1	;TB-bus
	.equ	ENDEC_CR1_THRU,	    0x00+ENDEC_SEL_CR1	;'through' mode
	.equ	ENDEC_CR1_LOOPBACK, 0x01+ENDEC_SEL_CR1	;loopback mode
	.equ	ENDEC_CR1_SHORTBACK,0x02+ENDEC_SEL_CR1	;short-loopback
	.equ	ENDEC_CR1_REPEAT,   0x03+ENDEC_SEL_CR1	;repeat mode

;ENDEC CR2 modes
	.equ	ENDEC_CR2_PARCON,   0x01+ENDEC_SEL_CR2	;'parity conversion'
	.equ	ENDEC_CR2_EVEN,	    0x02+ENDEC_SEL_CR2	;even/odd parity
	.equ	ENDEC_CR2_RESET,    0x04+ENDEC_SEL_CR2	;reset

;ENDEC CR3 modes
	.equ	ENDEC_CR3_SMRESET,  0x01+ENDEC_SEL_CR1	;reset smoother

;current ENDEC state from the ENDEC itself
	.equ	ENDEC_STATUS_UNKNOWN,0x0000
	.equ	ENDEC_STATUS_NLS,   0x1000
	.equ	ENDEC_STATUS_MLS,   0x2000
	.equ	ENDEC_STATUS_ILS,   0x3000
	.equ	ENDEC_STATUS_HLS,   0x4000
	.equ	ENDEC_STATUS_QLS,   0x5000
	.equ	ENDEC_STATUS_ALS,   0x6000
	.equ	ENDEC_STATUS_ELASTICITY,0x7000
	.equ	ENDEC_STATUS_MASK,  0x7000

	DEFWAIT	ENDEC_WAIT, 280		;cycles to touch ENDEC
	.equ	ENDEC_REC_NSEC,FE_REC	;recovery time--really 120

;delay to let the ENDEC recover
;   treg=scratch register, cycles=delay already available
 .macro	ENDEC_REC, treg, cycles, pf,
 .if	(((ENDEC_REC_NSEC)*MHZ+999)/1000-(cycles+0)+(pf+PFETCH)) < 0x80000000
	  NDELAY  treg, ENDEC_REC_NSEC, cycles+0
 .endif
 .endm



;CMT PAL output
	.equ	CMT_MASK,   0x7			;line state bits



;Non-volatile RAM commands
	.equ	NOVRAM_WRDS,0x80	;reset write enable latch
	.equ	NOVRAM_STO, 0x81	;store RAM to EEPROM
	.equ	NOVRAM_SLEEP,0x82	;sleep
	.equ	NOVRAM_WRITE,0x83	;write into RAM
	.equ	NOVRAM_WREN,0x84	;set write enable latch
	.equ	NOVRAM_RCL, 0x85	;recall EEPROM data into RAM
	.equ	NOVRAM_READ,0x86	;read from RAM

	.equ	SIZE_NOVRAM,16		;size in 16-bit words


;Front End RX descriptor
	.equ	RX_DESC_LEN,16		;descriptor length in bytes

	DEFBT	RX_DESC_E,  31		;E indicator
	DEFBT	RX_DESC_A,  30		;A indicator
	DEFBT	RX_DESC_C,  29		;C indicator
	DEFBT	RX_DESC_ERR,28		;error (CRC or invalid length)
	
	DEFBT	RX_DESC_DA, 27		;DA match
	DEFBT	RX_DESC_MAC,26		;MAC frame
	DEFBT	RX_DESC_SMT,25		;SMT frame
	DEFBT	RX_DESC_IMP,24		;implementor frame
	
	DEFBT	RX_DESC_FLSH,23		;flushed frame
	DEFBT	RX_DESC_ABT,22		;aborted frame
	DEFBT	RX_DESC_MIS,21		;missed frame
	DEFBT	RX_DESC_OVF,20		;receive frame overflow

	DEFBT	RX_DESC_F_LEN,0		;frame length in descriptor
	.equ	RX_DESC_F_LEN_WIDTH,16


	DEFBT	RX_DESC_LAST, RX_DESC_OVF_BT	;last bit in descriptor
	DEFBT	RX_DESC_OFFICIAL, RX_DESC_DA_BT	;last "official" bit
	.equ	RX_DESC_O_BAD, RX_DESC_OVF|RX_DESC_MIS|RX_DESC_OVF


;Front End TX descriptor
	.equ	TX_DESC, 3<<24		;always pad with 3 bytes

	.equ	TX_DESC_LEN, 16		;length



;delay a fixed number of nanoseconds, less a number of cycles
;   Change a special register to force any current loads or stores to be
;	completed.
;   Unfortunately, the Metaware assembler cannot do repeat blocks in macros.
;	It also does not understand signed arithmetic.
;   This macro promises to ensure that any external accesses will
;	have been finished for at least the requested number of nanoseconds.
;	It synchronizes the 29K by doing an extra fetch.
;   If the delay is short enough, and the extra number of cycles long enough
;	to ensure that no extra waiting is required, it does not wait.

; reg=scratch register
; nsec=required delay here or later
; cycles=clocks consumed by other code before the target is touched
; pf=nothing or -PFETCH if the memory cycle is know to have been started

	 .equ	SYNWAIT,1		;cycles used by synchronizing

	 .equ	NDELAY_LIM, 31
	 .equ	MIN_NDELAY_LIM, 3
	 .set	CUR_NDELAY_LIM, NDELAY_LIM
	 
 .macro	NDELAY, reg, nsec, cycles, pf,
 .if	(((nsec)*MHZ+999)/1000-(cycles)+(pf+PFETCH)) < 0x80000000
	  mtsrim  mmu,0xff
 .if	(((nsec)*MHZ+999)/1000-(cycles)-SYNWAIT) < 0x80000000
 .if	(((nsec)*MHZ+999)/1000-(cycles)-SYNWAIT) > CUR_NDELAY_LIM
 .if	((((nsec)*MHZ+999)/1000-(cycles)-SYNWAIT)/2) > 0xffff
	.print	" ** ERROR: @nsec@ does not fit in 16 bits"
	.err
 .endif
	  const reg,(((nsec)*MHZ+999)/1000-(cycles)-SYNWAIT)/2
	  jmpfdec reg, .
	   nop
 .else
 .if	0 != ((((nsec)*MHZ+999)/1000-(cycles)-SYNWAIT) & 16)
	  nop			;16
	  nop			;16
	  nop			;16
	  nop			;16
	  nop			;16
	  nop			;16
	  nop			;16
	  nop			;16
	  nop			;16
	  nop			;16
	  nop			;16
	  nop			;16
	  nop			;16
	  nop			;16
	  nop			;16
	  nop			;16
 .endif
 .if	0 != ((((nsec)*MHZ+999)/1000-(cycles)-SYNWAIT) & 8)
	  nop			;8
	  nop			;8
	  nop			;8
	  nop			;8
	  nop			;8
	  nop			;8
	  nop			;8
	  nop			;8
 .endif
 .if	0 != ((((nsec)*MHZ+999)/1000-(cycles)-SYNWAIT) & 4)
	  nop			;4
	  nop			;4
	  nop			;4
	  nop			;4
 .endif
 .if	0 != ((((nsec)*MHZ+999)/1000-(cycles)-SYNWAIT) & 2)
	  nop			;2
	  nop			;2
 .endif
 .if	0 != ((((nsec)*MHZ+999)/1000-(cycles)-SYNWAIT) & 1)
	  nop			;1
 .endif
 .endif
 .endif
 .endif
 .endm
