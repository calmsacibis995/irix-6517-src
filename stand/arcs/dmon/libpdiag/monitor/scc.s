#ident	"$Id: scc.s,v 1.2 1995/09/05 07:40:46 jeffs Exp $"
/**************************************************************************
 *									  *
 * 		 Copyright (C) 1993, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/


#define STANDALONE 1
#define _STANDALONE 1
#include "sys/asm.h"
/*
#include "machine/cp0.h"
#include "machine/cpu_board.h"
#include "machine/hd146818.h"
#include "machine/mk48t02.h"
#include "prom/prom.h"
*/
#include "sys/sbd.h"
#include "sys/regdef.h"
#include "scc.h"
#include "scc_cons.h"
#include "sys/z8530.h"


#define	MAX_BAUD_INDEX		11

		.extern	scc_baud_table

		.text

LEAF(scc_init)

		move	s4,ra

	/*
	 * Initialize Channel A
	 */
		li	s5,K1BASE|SCC_BASE	# set SCC pointer addr

		la	s6,SCCTable		# point to SCC table A start
		la	s7,TableEnd		# point to SCC table A end
		lw	s3,SCC_PTRA(s5)		# read once
		li	s1,SCCW_MIC_R		# master intr control reg
		li	s2,SCCW_RES_M		# reset both channel A and B
		sw	s1,SCC_PTRA(s5)		# write mic register number to SCC
		jal	FlushWB			# wait for a while
		sw	s2,SCC_PTRA(s5)		# force hardware reset
		jal	FlushWB			# wait for a while
		b	2f			# get first byte in table A
1:
		sw	s0,SCC_PTRA(s5)		# write register number to SCC
		addu	s6,1			# increment table pointer
		lbu	s0,0(s6)		# get SCC data
		nop				# delay slot
		sw	s0,SCC_PTRA(s5)		# write data to SCC
		addu	s6,1			# increment table pointer
		jal	FlushWB			# wait for a while
2:
		lbu	s0,0(s6)		# get SCC register number
		bltu	s6,s7,1b		# keep loading util reach end

		jal	FlushWB			# wait for a while

/*
		NVRAMADDR(NVADDR_LBAUD)
		lbu	s0,(v0)			# get baud rate
		ble	s0,MAX_BAUD_INDEX,1f
*/

		li	s0,10			# default to 9600 baud
1:
		sll	s0,1			# create index into scc_baud_table table
		lbu	s1,scc_baud_table(s0)	# get high
		lbu	s2,scc_baud_table+1(s0)	#   and low bytes

		li	s0,SCCW_BRLO_R
		sw	s0,SCC_PTRA(s5)		# select write register 12
		jal	FlushWB
		sw	s2,SCC_PTRA(s5)		# write low byte of time constant
		jal	FlushWB
		li	s0,SCCW_BRHI_R
		sw	s0,SCC_PTRA(s5)		# select write register 13
		jal	FlushWB
		sw	s1,SCC_PTRA(s5)		# write high byte of time constant
		jal	FlushWB
		li	s0,SCCW_AUX_R
		li	s1,SCCW_AUX_BRfromPClock | SCCW_AUX_BRGenEnable
		sw	s0,SCC_PTRA(s5)		# select write register 14
		jal	FlushWB
		sw	s1,SCC_PTRA(s5)
		jal	FlushWB

	/*
	 * Initialize Channel B
	 */
		la	s6,SCCTable		# point to SCC table A start
		la	s7,TableEnd		# point to SCC table A end
 #		lw	s3,SCC_PTRB(s5)		# read once
		b	2f			# get first byte in table A
1:
		sw	s0,SCC_PTRB(s5)		# write register number to SCC
		addu	s6,1			# increment table pointer
		lbu	s0,0(s6)		# get SCC data
		nop				# delay slot
		sw	s0,SCC_PTRB(s5)		# write data to SCC
		addu	s6,1			# increment table pointer
		jal	FlushWB			# wait for a while
2:
		lbu	s0,0(s6)		# get SCC register number
		bltu	s6,s7,1b		# keep loading util reach end

		jal	FlushWB			# wait for a while

/*
		NVRAMADDR(NVADDR_RBAUD)
		lbu	s0,(v0)			# get baud rate
		ble	s0,MAX_BAUD_INDEX,1f
*/

		li	s0,10			# default to 9600 baud
1:
		sll	s0,1			# create index into scc_baud_table table
		lbu	s1,scc_baud_table(s0)	# get high
		lbu	s2,scc_baud_table+1(s0)	#   and low bytes

		li	s0,SCCW_BRLO_R
		sw	s0,SCC_PTRB(s5)		# select write register 12
		jal	FlushWB
		sw	s2,SCC_PTRB(s5)		# write low byte of time constant
		jal	FlushWB
		li	s0,SCCW_BRHI_R
		sw	s0,SCC_PTRB(s5)		# select write register 13
		jal	FlushWB
		sw	s1,SCC_PTRB(s5)		# write high byte of time constant
		jal	FlushWB
		li	s0,SCCW_AUX_R
		li	s1,SCCW_AUX_BRfromPClock | SCCW_AUX_BRGenEnable
		sw	s0,SCC_PTRB(s5)		# select write register 14
		jal	FlushWB
		sw	s1,SCC_PTRB(s5)
		jal	FlushWB

		j	s4

END(scc_init)

/*
 * pon_io.s - register-based power-on diagnostic output primitives
 *
 */
#define	DELAY	li v0,100; 99: sub v0,1; bnez v0,99b

#define	STORE(offset,value)	DELAY; li t1,offset; sb t1,0(t0); DELAY; li t1,value; sb t1,0(t0)
LEAF(pon_initio)
	move	t5,ra

	li	t0,PHYS_TO_K1(0xbfbd9833)

	STORE(WR9, WR9_HW_RST)		# hardware reset
	STORE(WR4, WR4_X16_CLK|WR4_1_STOP)	# 16x clock, 1 stop bit
	STORE(WR3, WR3_RX_8BIT)		# 8 bit Rx character
	STORE(WR5, WR5_TX_8BIT|WR5_RTS|WR5_DTR)		# 8 bit Tx character
	STORE(WR11, WR11_RCLK_BRG|WR11_TCLK_BRG)	# clock source from BRG
	STORE(WR12, 0x0a)		# BRG time constant, lower byte, 9600
	STORE(WR13, 0x00)		# BRG time constant, upper byte, 9600
	STORE(WR14, WR14_BRG_ENBL)	# enable BRG
	STORE(WR3, WR3_RX_8BIT|WR3_RX_ENBL)	# enable receiver
	STORE(WR5, WR5_TX_8BIT|WR5_TX_ENBL|WR5_RTS|WR5_DTR)	# enable transmitter

	j	t5
	END(pon_initio)

LEAF(Scc_Local)

		move	t4,ra

	/*
	 * Initialize Channel A
	 */
		li	t5,K1BASE|SCC_BASE	# set SCC pointer addr

		la	t6,LoopTable1		# point to SCC table A start
		la	t7,LoopTable2		# point to SCC table A end
		lw	t3,SCC_PTRA(t5)		# read once
		li	t1,SCCW_MIC_R		# master intr control reg
		li	t2,SCCW_RES_M		# reset both channel A and B
		sw	t1,SCC_PTRA(t5)		# write mic register number to SCC
		jal	FlushWB			# wait for a while
		sw	t2,SCC_PTRA(t5)		# force hardware reset
		jal	FlushWB			# wait for a while
		b	2f			# get first byte in table A
1:
		sw	t0,SCC_PTRA(t5)		# write register number to SCC
		addu	t6,1			# increment table pointer
		lbu	t0,0(t6)		# get SCC data
		nop				# delay slot
		sw	t0,SCC_PTRA(t5)		# write data to SCC
		addu	t6,1			# increment table pointer
		jal	FlushWB			# wait for a while
2:
		lbu	t0,0(t6)		# get SCC register number
		bltu	t6,t7,1b		# keep loading util reach end

		jal	FlushWB			# wait for a while

/*
		NVRAMADDR(NVADDR_LBAUD)
		lbu	t0,(v0)			# get baud rate
		ble	t0,MAX_BAUD_INDEX,1f
*/

		li	t0,10			# default to 9600 baud
1:
		sll	t0,1			# create index into scc_baud_table table
		lbu	t1,scc_baud_table(t0)	# get high
		lbu	t2,scc_baud_table+1(t0)	#   and low bytes

		li	t0,SCCW_BRLO_R
		sw	t0,SCC_PTRA(t5)		# select write register 12
		jal	FlushWB
		sw	t2,SCC_PTRA(t5)		# write low byte of time constant
		jal	FlushWB
		li	t0,SCCW_BRHI_R
		sw	t0,SCC_PTRA(t5)		# select write register 13
		jal	FlushWB
		sw	t1,SCC_PTRA(t5)		# write high byte of time constant
		jal	FlushWB

		la	t6,LoopTable2		# point to SCC table A start
		la	t7,LoopTableEnd		# point to SCC table A end
		b	2f			# get first byte in table A
1:
		sw	t0,SCC_PTRA(t5)		# write register number to SCC
		addu	t6,1			# increment table pointer
		lbu	t0,0(t6)		# get SCC data
		nop				# delay slot
		sw	t0,SCC_PTRA(t5)		# write data to SCC
		addu	t6,1			# increment table pointer
		jal	FlushWB			# wait for a while
2:
		lbu	t0,0(t6)		# get SCC register number
		bltu	t6,t7,1b		# keep loading util reach end

		jal	FlushWB			# wait for a while

	/*
	 * Initialize Channel B
	 */
		la	t6,LoopTable1		# point to SCC table A start
		la	t7,LoopTable2		# point to SCC table A end
		lw	t3,SCC_PTRB(t5)		# read once
		b	2f			# get first byte in table A
1:
		sw	t0,SCC_PTRB(t5)		# write register number to SCC
		addu	t6,1			# increment table pointer
		lbu	t0,0(t6)		# get SCC data
		nop				# delay slot
		sw	t0,SCC_PTRB(t5)		# write data to SCC
		addu	t6,1			# increment table pointer
		jal	FlushWB			# wait for a while
2:
		lbu	t0,0(t6)		# get SCC register number
		bltu	t6,t7,1b		# keep loading util reach end

		jal	FlushWB			# wait for a while

/*
		NVRAMADDR(NVADDR_RBAUD)
		lbu	t0,(v0)			# get baud rate
		ble	t0,MAX_BAUD_INDEX,1f

*/
		li	t0,10			# default to 9600 baud
1:
		sll	t0,1			# create index into scc_baud_table table
		lbu	t1,scc_baud_table(t0)	# get high
		lbu	t2,scc_baud_table+1(t0)	#   and low bytes

		li	t0,SCCW_BRLO_R
		sw	t0,SCC_PTRB(t5)		# select write register 12
		jal	FlushWB
		sw	t2,SCC_PTRB(t5)		# write low byte of time constant
		jal	FlushWB
		li	t0,SCCW_BRHI_R
		sw	t0,SCC_PTRB(t5)		# select write register 13
		jal	FlushWB
		sw	t1,SCC_PTRB(t5)		# write high byte of time constant
		jal	FlushWB

		la	t6,LoopTable2		# point to SCC table A start
		la	t7,LoopTableEnd		# point to SCC table A end
		b	2f			# get first byte in table A
1:
		sw	t0,SCC_PTRB(t5)		# write register number to SCC
		addu	t6,1			# increment table pointer
		lbu	t0,0(t6)		# get SCC data
		nop				# delay slot
		sw	t0,SCC_PTRB(t5)		# write data to SCC
		addu	t6,1			# increment table pointer
		jal	FlushWB			# wait for a while
2:
		lbu	t0,0(t6)		# get SCC register number
		bltu	t6,t7,1b		# keep loading util reach end

		jal	FlushWB			# wait for a while

		j	t4

END(Scc_Local)


/* 
 * This test writes to SCC Channel B, then reads it back through external
 * loopback cable.  Each character is verified and put out to channel A
 * for displaying in the terminal.  This test is to make sure channel B 
 * is good for serial downloading and modem communication.
 */

LEAF(scc_loopback)

		move	t7, ra
		la	t2, scc_lp_msg		# get address of test msg
next:
		move	t5,zero			# zero timeout counter
		lbu	t3,0(t2)		# load test msg char
		and	t3,0xff			# mask
1:
		addu	t5,t5,1			# increment timeout counter
		bgt	t5,4096,4f		# time out!
		li	t1,SCC_PTRB		# get channel B address
		lw	t0,0(t1)		# get channel B status
		and	t0,t0,SCCR_STATUS_TxReady
						# see if TBE is set
		beq	t0,zero,1b		# poll again if not set
		sw	t3,SCC_DATAB		# put to SCC channel B
		jal	FlushWB

		move	t5,zero			# zero timeout counter
2:
		addu	t5,t5,1			# increment timeout counter
		bgt	t5,4096,4f		# time out!
                li      t1,SCC_PTRB          # get channel B address
                lw      t0,0(t1)                # get channel B status
                and     t0,t0,SCCR_STATUS_RxDataAvailable
                                                # see if DataAvailable set
                beq     t0,zero,2b              # poll again if not set
		lw	t4,SCC_DATAB		# get data
		and	t4,0xff			# mask
		jal	FlushWB
		
		bne	t3,t4,4f		# compare failed
3:
		and	t4,0x7f			# no parity bit
                li      t1,SCC_PTRA          # get channel B address
                lw      t0,0(t1)                # get channel B status
                and     t0,t0,SCCR_STATUS_TxReady
                                                # see if TBE is set
                beq     t0,zero,3b              # poll again if not set
		sw	t4,SCC_DATAA		# print char
		jal	FlushWB
		addu	t2,t2,1			# next char
		bne	t4,zero,next		# end of test msg
4:
		j	t7			# return to the calling addr
		
END(scc_loopback)


		.data

/*
 * Table for initializing SCC (Z8530) control registers.
 *
 * In each entry of the table, the first byte is the register offset
 * from WR0 (pointer register) and the second byte is the data to be loaded
 * into the register.  Both channel A and channel B are supported.
 */
SCCTable:
		.byte	SCCW_CMD_R
		.byte	SCCW_CMD_CMD_Null
		.byte	SCCW_CMD_R
		.byte	SCCW_CMD_CMD_Null
		.byte	SCCW_CMD_R
		.byte	SCCW_CMD_CMD_Null
		.byte	SCCW_MIC_R
		.byte	SCCW_CMD_CMD_Null
		.byte	SCCW_RCV_R
		.byte	SCCW_RCV_8Bits | SCCW_RCV_RxEn
		.byte	SCCW_MISC_R
		.byte	SCCW_MISC_CLK_16 | SCCW_MISC_SB_1
		.byte	SCCW_TXMDM_R
		.byte	SCCW_TXMDM_LabeledDTR | SCCW_TXMDM_8Bits | SCCW_TXMDM_TxEnable | SCCW_TXMDM_LabeledRTS
		.byte	SCCW_CLK_R
		.byte	SCCW_CLK_RxC_BRGen | SCCW_CLK_TxC_BRGen | SCCW_CLK_DriveTRxC | SCCW_CLK_TRxC_BRGen
TableEnd:

LoopTable1:
		.byte	SCCW_CMD_R
		.byte	SCCW_CMD_CMD_Null
		.byte	SCCW_MISC_R
		.byte	SCCW_MISC_CLK_16 | SCCW_MISC_SB_1
		.byte	SCCW_VECTOR_R
		.byte	0
		.byte	SCCW_RCV_R
		.byte	SCCW_RCV_8Bits
		.byte	SCCW_TXMDM_R
		.byte	SCCW_TXMDM_LabeledDTR | SCCW_TXMDM_8Bits | SCCW_TXMDM_LabeledRTS
		.byte	SCCW_SYNC_1st_R
		.byte	0
		.byte	SCCW_SYNC_2nd_R
		.byte	0
		.byte	SCCW_MIC_R
		.byte	0
		.byte	SCCW_ENCODE_R
		.byte	0
		.byte	SCCW_CLK_R
		.byte	SCCW_CLK_RxC_BRGen | SCCW_CLK_TxC_BRGen | SCCW_CLK_DriveTRxC | SCCW_CLK_TRxC_BRGen
LoopTable2:
		.byte	SCCW_AUX_R
		.byte	SCCW_AUX_CMD_Null
		.byte	SCCW_RCV_R
		.byte	SCCW_RCV_8Bits | SCCW_RCV_RxEn
		.byte	SCCW_TXMDM_R
		.byte	SCCW_TXMDM_LabeledDTR | SCCW_TXMDM_8Bits | SCCW_TXMDM_TxEnable | SCCW_TXMDM_LabeledRTS
		.byte	SCCW_CMD_R
		.byte	0x80			# incorrect define in scc_cons.h
		.byte	SCCW_AUX_R
		.byte	SCCW_AUX_Loopback | SCCW_AUX_BRfromPClock | SCCW_AUX_BRGenEnable
		.byte	SCCW_ENAB_R
		.byte	0
		.byte	SCCW_EXTINTR_R
		.byte	0
		.byte	SCCW_CMD_R
		.byte	SCCW_CMD_CMD_ResetExtIntr
		.byte	SCCW_CMD_R
		.byte	SCCW_CMD_CMD_ResetExtIntr
LoopTableEnd:

		.align 2
EXPORT(scc_baud_table)
		.half	B_CONST(75)
		.half	B_CONST(110)
		.half	B_CONST(134)
		.half	B_CONST(150)
		.half	B_CONST(300)
		.half	B_CONST(600)
		.half	B_CONST(1200)
		.half	B_CONST(1800)
		.half	B_CONST(2400)
		.half	B_CONST(4800)
		.half	B_CONST(9600)
		.half	B_CONST(19200)

		.align 2
scc_lp_msg:     .asciiz "\n\rSCC(B) external loopback test...Done.\n\r"

