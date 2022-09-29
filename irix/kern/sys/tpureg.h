/**************************************************************************
 *									  *
 * 		 Copyright (C) 1998, Silicon Graphics, Inc		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

/********************************************************************************
 *										*
 * sys/tpureg.h - Mesa TPU register definitions.				*
 *										*
 ********************************************************************************/

#ifndef __SYS_TPUREG_H__
#define __SYS_TPUREG_H__

#ifdef __cplusplus
extern "C" {
#endif

#ident  "$Id: tpureg.h,v 1.1 1998/06/10 16:03:08 pww Exp $"

#include "sys/xtalk/xtalk.h"
#include "sys/xtalk/xwidget.h"

/* 
 * Byte offset of member in structure.
 */
#define OFFSET(struct, mem)	((__uint64_t) &(((struct *) 0) -> mem))

/*
 * Crosstalk hardware values.
 */
#define	TPU_WIDGET_PART_NUM	0xc202
#define	TPU_WIDGET_MFGR_NUM	0x024
#define TPU_WIDGET_NIC		0x00005c /* Number In a Can register */

/*
 *	All registers on the TPU Client device are 64 bits wide.
 */
typedef	__uint64_t		tpu_reg_t;

/*
 *	Standard xtalk widget registers and interrupt registers.
 */
#define	TPU_XTK_REGS_OFFSET	0x0

typedef volatile struct tpu_xtk_regs_s {

	/* Standard widget configuration */
	widget_cfg_t		x_cfg;			/* 0x0_0000 Standard widget registers */

	/* helper fieldnames for accessing tpu widget */
#define x_id			x_cfg.w_id			/* 0x0_0004 Identification register */
#define x_status		x_cfg.w_status			/* 0x0_000c Crosstalk Status register */
#define x_err_upper_addr	x_cfg.w_err_upper_addr		/* 0x0_0014 Error Upper Address reg */
#define x_err_lower_addr	x_cfg.w_err_lower_addr		/* 0x0_001c Error Address register */
#define x_control		x_cfg.w_control			/* 0x0_0024 Crosstalk Control reg */
#define x_req_timeout		x_cfg.w_req_timeout		/* 0x0_002c Request Time-out Value */
#define x_intdest_upper_addr	x_cfg.w_intdest_upper_addr	/* 0x0_0034 Intrrupt Destin Addr Upper */
#define x_intdest_lower_addr	x_cfg.w_intdest_lower_addr	/* 0x0_003c Interrupt Destination Addr */
#define x_err_cmd_word		x_cfg.w_err_cmd_word		/* 0x0_0044 Error Command Word reg */
#define x_llp_cfg		x_cfg.w_llp_cfg			/* 0x0_004c LLP Configuration reg */
#define x_tflush		x_cfg.w_tflush			/* 0x0_0054 Target Flush Register */

	/* Number in a can */
	widgetreg_t             _pad_0058;		/* 0x0_0058 */
	widgetreg_t             x_nic;			/* 0x0_005c Number In a Can register */

	/* TPU specific registers */
	widgetreg_t             _pad_0060;		/* 0x0_0060 */
	widgetreg_t             x_ede;			/* 0x0_0064 Error Data Enable reg */
	widgetreg_t             _pad_0068[0x26];	/* 0x0_0068 -> 0x_00fc */
	widgetreg_t             _pad_0100;		/* 0x0_0100 */
	widgetreg_t             x_int_status;		/* 0x0_0104 Interrupt Status register */
	widgetreg_t             _pad_0108;		/* 0x0_0108 */
	widgetreg_t             x_int_enable;		/* 0x0_010c Interrupt Enable register */
	widgetreg_t             _pad_0110;		/* 0x0_0110 */
	widgetreg_t             x_int_status_reset;	/* 0x0_0114 Reset Interrupt Status Register */
	widgetreg_t             _pad_0118[0x6];		/* 0x0_0118 -> 0x_012c */
	widgetreg_t             _pad_0130;		/* 0x0_0130 */
	widgetreg_t             x_int_addr0;		/* 0x0_0134 Aux Interrupt 0 Addr reg */
	widgetreg_t             _pad_0138;		/* 0x0_0138 */
	widgetreg_t             x_int_addr1;		/* 0x0_013c Aux Interrupt 1 Addr reg */
	widgetreg_t             _pad_0140;		/* 0x0_0140 */
	widgetreg_t             x_int_addr2;		/* 0x0_0144 Aux Interrupt 2 Addr reg */
	widgetreg_t             _pad_0148;		/* 0x0_0148 */
	widgetreg_t             x_int_addr3;		/* 0x0_014c Aux Interrupt 3 Addr reg */
	widgetreg_t             _pad_0150;		/* 0x0_0150 */
	widgetreg_t             x_int_addr4;		/* 0x0_0154 Aux Interrupt 4 Addr reg */
	widgetreg_t             _pad_0158;		/* 0x0_0158 */
	widgetreg_t             x_int_addr5;		/* 0x0_015c Aux Interrupt 5 Addr reg */
	widgetreg_t             _pad_0160;		/* 0x0_0160 */
	widgetreg_t             x_int_addr6;		/* 0x0_0164 Aux Interrupt 6 Addr reg */
	widgetreg_t		_pad_0168[0x6];		/* 0x0_0168 -> 0x_017c */
	widgetreg_t		_pad_0180;		/* 0x0_0180 */
	widgetreg_t		x_sense;		/* 0x0_0184 User Definable Sense reg */
	widgetreg_t		_pad_0188;		/* 0x0_0188 */
	widgetreg_t		x_leds;			/* 0x0_018c User Definable Enable reg */

} tpu_xtk_regs_t;

/*
 *	Address translation table registers.
 */
#define	TPU_ATT_REGS_OFFSET	0x80000
#define TPU_ATT_SIZE		128

typedef volatile struct tpu_att_regs_s {
	widgetreg_t             _pad_0000;		/* 0x8_0000 */
	widgetreg_t             a_config;		/* 0x8_0004 Address Translation Configuration reg */
	widgetreg_t             _pad_0008;		/* 0x8_0008 */
	widgetreg_t             a_diag;			/* 0x8_000c Address Translation Diagnostic reg */
	widgetreg_t             _pad_0010[0xfc];	/* 0x8_0010 -> 0x8_03fc */
	tpu_reg_t		a_atte[TPU_ATT_SIZE];	/* 0x8_0400 Address Translation Table */

} tpu_att_regs_t;

/*
 *	DMA registers.
 */
#define	TPU_DMA0_REGS_OFFSET	0x100000
#define	TPU_DMA1_REGS_OFFSET	0x200000

typedef volatile struct tpu_dma_regs_s {
	tpu_reg_t		d_config0;		/* 0x10_0000 Configuration Reg 0 */
	tpu_reg_t		d_config1;		/* 0x10_0008 Configuration Reg 1 */
	tpu_reg_t		d_status;		/* 0x10_0010 Status register */
	tpu_reg_t		d_diag_mode;		/* 0x10_0018 Diagnostic Mode register */
	tpu_reg_t		d_perf_timer;		/* 0x10_0020 Performance Timer reg */
	tpu_reg_t		d_perf_count;		/* 0x10_0028 Performance Counter reg */
	tpu_reg_t		d_perf_config;		/* 0x10_0030 Performance Config reg */

} tpu_dma_regs_t;

/*
 *	LDI registers.
 */
#define	TPU_LDI_REGS_OFFSET	0x300000
#define TPU_LDI_I_SIZE		64
#define	TPU_LDI_I_UPPER		0
#define	TPU_LDI_I_LOWER		1

typedef volatile struct tpu_ldi_regs_s {

	tpu_reg_t		_pad_0000[0x5c];	/*    0x30_0000 -> 0x30_02d8 */
	tpu_reg_t		riscIOpcode;		/*  0 0x30_02e0 Risc Loaded Op Instruction */
	tpu_reg_t		riscMode;		/*  1 0x30_02e8 using Risc to operate LSP */
	tpu_reg_t		xRiscSadr;		/*  2 0x30_02f0 Risc loaded starting addr */
	tpu_reg_t		coefBs;			/*  3 0x30_02f8 Coef Block Size */
	tpu_reg_t		coefInc;		/*  4 0x30_0300 Coef Incrementer */
	tpu_reg_t		coefInit;		/*  5 0x30_0308 Coef Initial data */
	tpu_reg_t		dataBs;			/*  6 0x30_0310 Data Block Size */
	tpu_reg_t		saBlkInc;		/*  7 0x30_0318 Start Addr Blk Incrementer */
	tpu_reg_t		saBlkSize;		/*  8 0x30_0320 Start Addr Blk Size */
	tpu_reg_t		saBlkInit;		/*  9 0x30_0328 Start Addr Blk Initial */
	tpu_reg_t		statusReg;		/* 10 0x30_0330 Status readback register */
	tpu_reg_t		sampInit;		/* 11 0x30_0338 Receive sample data initial */
	tpu_reg_t		vectInitInc;		/* 12 0x30_0340 Rec SA Vector init Incr */
	tpu_reg_t		vectInit;		/* 13 0x30_0348 Rec SA Vector initial */
	tpu_reg_t		vectInc;		/* 14 0x30_0350 Rec SA Vector incrementer */
	tpu_reg_t		errorMask;		/* 15 0x30_0358 Error interrupt mask */
	tpu_reg_t		lbaBlkSize;		/* 16 0x30_0360 LBA block size */
	tpu_reg_t		loopCnt;		/* 17 0x30_0368 Loop counter for i-que */
	tpu_reg_t		diagReg;		/* 18 0x30_0370 Diagnostic Readback register */
	tpu_reg_t		iSource;		/* 19 0x30_0378 Timer/barrier source reg */
	tpu_reg_t		pipeInBus[8];		/* 20 0x30_0380 -> 27 0x30_03b8 xmit Load pipe In bus size */
	tpu_reg_t		rRiscSadr;		/* 28 0x30_03c0 */
	tpu_reg_t		sampInc;		/* 29 0x30_03c8 Receive sample increment */
	tpu_reg_t		oLineSize;		/* 30 0x30_03d0 Receive output line size */
	tpu_reg_t		iMask;			/* 31 0x30_03d8 */
	tpu_reg_t		tFlag;			/* 32 0x30_03e0 */
	tpu_reg_t		bFlag;			/* 33 0x30_03e8 */
	tpu_reg_t		diagPart;		/* 34 0x30_03f0 */
	tpu_reg_t		jmpCnd;			/* 35 0x30_03f8 */
	tpu_reg_t		inst[TPU_LDI_I_SIZE][2];/*    0x30_0400 Instruction memory*/

} tpu_ldi_regs_t;


/*
 * Standard xtalk widget registers and interrupt registers.
 */

/* 0x0_0004 Identification register */

#define TPU_REV_NUM_SHFT		(28)
#define TPU_REV_NUM_MSK			(0xfull << TPU_REV_NUM_SHFT)
#define TPU_PART_NUM_SHFT		(12)
#define TPU_PART_NUM_MSK		(0xffffull << TPU_PART_NUM_SHFT)
#define TPU_MFG_NUM_SHFT		(1)
#define TPU_MFG_NUM_MSK			(0x3ffull << TPU_MFG_NUM_SHFT)
#define TPU_PART_NUM(id)		(((id) & TPU_PART_NUM_MSK) >> TPU_PART_NUM_SHFT)
#define TPU_REV_NUM(id)			(((id) & TPU_REV_NUM_MSK) >> TPU_REV_NUM_SHFT)
#define TPU_MFG_NUM(id)			(((id) & TPU_MFG_NUM_MSK) >> TPU_MFG_NUM_SHFT)
#define TPU_ID_REG_MSK			((TPU_REV_NUM_MSK  << 32) | \
					 (TPU_PART_NUM_MSK << 32) | \
					 (TPU_MFG_NUM_MSK  << 32) | \
					 TPU_REV_NUM_MSK	  | \
					 TPU_PART_NUM_MSK	  | \
					 TPU_MFG_NUM_MSK	  )

#define TPU_REV_NUM_REV_0		(0x0)
#define TPU_PART_NUM_REV_0		(0xc202)
#define TPU_MFG_NUM_REV_0		(0x24)
#define TPU_ID_REV_0			((TPU_PART_NUM (TPU_PART_NUM_REV_0)) | \
					 (TPU_REV_NUM  (TPU_REV_NUM_REV_0))  | \
					 (TPU_MFG_NUM  (TPU_MFG_NUM_REV_0))  | \
					 1				     )

/* 0x0_000c crosstalk Status register */

#define TPU_LLP_REC_CNT_SHFT		(24)
#define TPU_LLP_REC_CNT_MSK		(0xff << TPU_LLP_REC_CNT_SHFT)
#define TPU_LLP_TX_CNT_SHFT		(16)
#define TPU_LLP_TX_CNT_MSK		(0xff << TPU_LLP_TX_CNT_SHFT)
#define TPU_TX_CREDIT_CNT_SHFT		(12)
#define TPU_TX_CREDIT_CNT_MSK		(0xf << TPU_TX_CREDIT_CNT_SHFT)
#define TPU_RX_CREDIT_CNT_SHFT		(10)
#define TPU_RX_CREDIT_CNT_MSK		(0x3 << TPU_RX_CREDIT_CNT_SHFT)
#define TPU_LLP_BITMODE_8		(1 << 6)
#define TPU_LDI_ENABLE			(1 << 5)
#define TPU_PENDING_SHFT		(0)
#define TPU_PENDING_MSK			(0x1f << TPU_PENDING_SHFT)
#define TPU_STATUS_REG_MSK		(TPU_LLP_BITMODE_8     | \
					 TPU_LDI_ENABLE	       | \
					 TPU_LLP_REC_CNT_MSK   | \
					 TPU_LLP_TX_CNT_MSK    | \
					 TPU_TX_CREDIT_CNT_MSK | \
					 TPU_RX_CREDIT_CNT_MSK | \
					 TPU_PENDING_MSK       )

/* 0x0_0014 Error Upper Address reg */

#define TPU_ERR_ADDR_UPPER_MSK		(0x0000ffff)
#define TPU_ERR_ADDR_UPPER_REG_MSK	TPU_ERR_ADDR_UPPER_MSK

/* 0x0_001c Error Address register */

#define TPU_ERR_ADDR_MSK		(0x0000ffffffffffff)
#define TPU_ERR_ADDR_REG_MSK		TPU_ERR_ADDR_MSK

/* 0x0_0024 Crosstalk Control reg */

#define TPU_LLP_XBAR_CREDIT_SHFT	(24)
#define TPU_LLP_XBAR_CREDIT_MSK		(0xff << TPU_LLP_XBAR_CREDIT_SHFT)
#define TPU_SOFT_RESET			(1 << 20)
#define TPU_TAG_MODE			(1 << 19)
#define TPU_GBR_MODE			(1 << 18)
#define TPU_WR_RESP_MODE		(1 << 17)
#define TPU_FORCE_BAD_PKT		(1 << 16)
#define TPU_CLR_RLLP_CNT		(1 << 11)
#define TPU_CLR_TLLP_CNT		(1 << 10)
#define TPU_SYS_END			(1 << 9)
#define TPU_MAX_TRANS_SHFT		(4)
#define TPU_MAX_TRANS_MSK		(0x1f << TPU_MAX_TRANS_SHFT)
#define TPU_WIDGET_ID_SHFT		(0)
#define TPU_WIDGET_ID_MSK		(0xf << TPU_WIDGET_ID_SHFT)
#define TPU_CONTROL_REG_MSK		(TPU_SOFT_RESET		 | \
					 TPU_TAG_MODE		 | \
					 TPU_GBR_MODE		 | \
					 TPU_WR_RESP_MODE	 | \
					 TPU_FORCE_BAD_PKT	 | \
					 TPU_LLP_XBAR_CREDIT_MSK | \
					 TPU_CLR_RLLP_CNT	 | \
					 TPU_CLR_TLLP_CNT	 | \
					 TPU_SYS_END		 | \
					 TPU_MAX_TRANS_MSK	 | \
					 TPU_WIDGET_ID_MSK)

/* 0x0_002c Request Time-out Value */

#define TPU_REQ_TO_MSK			0x000fffff
#define TPU_REQ_TO_REG_MSK		TPU_REQ_TO_MSK

/* 0x0_0034 Intrrupt Destin Addr Upper */

#define TPU_INT_ADDR_UPPER_VECTOR_SHFT	(24)
#define TPU_INT_ADDR_UPPER_VECTOR_MSK	(0xffull << TPU_INT_ADDR_UPPER_VECTOR_SHFT)
#define TPU_INT_ADDR_UPPER_TARGET_ID_SHFT (16)
#define TPU_INT_ADDR_UPPER_TARGET_ID_MSK (0xfull << TPU_INT_ADDR_UPPER_TARGET_ID_SHFT)
#define TPU_INT_ADDR_UPPER_MSK		    (0x0000ffffull)
#define TPU_INT_ADDR_UPPER_REG_MSK	(TPU_INT_ADDR_UPPER_VECTOR_MSK	  |\
					 TPU_INT_ADDR_UPPER_TARGET_ID_MSK |\
					 TPU_INT_ADDR_UPPER_MSK)

/* 0x0_003c Interrupt Destination Addr */

#define TPU_INT_VECTOR_SHFT		(56)
#define TPU_INT_VECTOR_MSK		(0xffull << TPU_INT_VECTOR_SHFT)
#define TPU_TARGET_ID_SHFT		(48)
#define TPU_TARGET_ID_MSK		(0xfull << TPU_TARGET_ID_SHFT)
#define TPU_INT_ADDR_SHFT		(0)
#define TPU_INT_ADDR_MSK		(0xffffffffffffull << TPU_INT_ADDR_SHFT)
#define TPU_INT_DEST_ADDR_REG_MSK	(TPU_INT_VECTOR_MSK | \
					 TPU_TARGET_ID_MSK  | \
					 TPU_INT_ADDR_MSK   )

/* 0x0_0044 Error Command Word reg */

#define TPU_DATA_ENABLE_SHFT		(32)
#define TPU_DATA_ENABLE_MSK		(0xffffffffull << TPU_DATA_ENABLE_SHFT)
#define TPU_DIDN_SHFT			(28)
#define TPU_DIDN_MSK			(0xf << TPU_DIDN_SHFT)
#define TPU_SIDN_SHFT			(24)
#define TPU_SIDN_MSK			(0xf << TPU_SIDN_SHFT)
#define TPU_PACTYP_SHFT			(20)
#define TPU_PACTYP_MSK			(0xf << TPU_PACTYP_SHFT)
#define TPU_TNUM_SHFT			(15)
#define TPU_TNUM_MSK			(0x1f << TPU_TNUM_SHFT)
#define TPU_COHERENT			(1 << 14)
#define TPU_DS_SHFT			(12)
#define TPU_DS_MSK			(0x3 << TPU_DS_SHFT)
#define TPU_DATA_SIZE_SHFT		TPU_DS_SHFT
#define TPU_DATA_SIZE_MSK		TPU_DS_MSK 
#define TPU_GBR				(1 << 11)
#define TPU_VBPM			(1 << 10
#define TPU_ERROR			(1 << 9)
#define TPU_BARRIER			(1 << 8)
#define TPU_ERR_CMD_WORD_REG_MSK	(TPU_DATA_ENABLE_MSK | \
					 TPU_DIDN_MSK	     | \
					 TPU_SIDN_MSK	     | \
					 TPU_PACTYP_MSK	     | \
					 TPU_TNUM_MSK	     | \
					 TPU_COHERENT	     | \
					 TPU_DS_MSK	     | \
					 TPU_DATA_SIZE_MSK   | \
					 TPU_GBR	     | \
					 TPU_VBPM	     | \
					 TPU_ERROR	     | \
					 TPU_BARRIER	     )

/* 0x0_004c LLP Configuration reg */

#define TPU_LLP_MAX_RETRY_SHFT		(16)
#define TPU_LLP_MAX_RETRY_MSK		(0x3ff << TPU_LLP_MAX_RETRY_SHFT)
#define TPU_LLP_NULL_TO_SHFT		(10)
#define TPU_LLP_NULL_TO_MSK		(0x3f << TPU_LLP_NULL_TO_SHFT)
#define TPU_LLP_MAX_BURST_SHFT		(0)
#define TPU_LLP_MAX_BURST_MSK		(0x3ff << TPU_LLP_MAX_BURST_SHFT)
#define TPU_LLP_CONFIG_REG_MSK		(TPU_LLP_MAX_RETRY_MSK | \
					 TPU_LLP_NULL_TO_MSK   | \
					 TPU_LLP_MAX_BURST_MSK )

/* 0x0_0054 Target Flush Register */

#define TPU_T_FLUSH_REG_MSK		(0)

/* 0x0_005c Number In a Can register */

#define TPU_NIC_BMP_SHFT		(10)
#define TPU_NIC_BMP_MSK			(0x3ff << TPU_NIC_BMP_SHFT)
#define TPU_NIC_OFFSET_SHFT		(2)
#define TPU_NIC_OFFSET_MSK		(0xff << TPU_NIC_OFFSET_SHFT)
#define TPU_NIC_DATA_VALID		(1ull << 1)
#define TPU_NIC_DATA			(1ull << 0)
#define TPU_NIC_REG_MSK			(TPU_NIC_BMP_MSK    | \
					 TPU_NIC_OFFSET_MSK | \
					 TPU_NIC_DATA_VALID | \
					 TPU_NIC_DATA	    )

/* 0x0_0064 Error Data Enable reg */

#define TPU_ERR_DATA_ENABLE_SHFT	(0)
#define TPU_ERR_DATA_ENABLE_MSK		(0xffffffffull << TPU_ERR_DATA_ENABLE_SHFT)
#define TPU_ERR_DATA_ENABLE_REG_MSK	TPU_ERR_DATA_ENABLE_MSK

/* 0x0_0104 Interrupt Status register */
/* 0x0_010c Interrupt Enable register */
/* 0x0_0114 Reset Interrupt Status Register */

#define TPU_LINK_RESET			(1 << 30)
#define TPU_UNEXP_RESP			(1 << 29)
#define TPU_BAD_XRSP_PKT		(1 << 28)
#define TPU_BAD_XREQ_PKT		(1 << 27)
#define TPU_RESP_ERR			(1 << 26)
#define TPU_REQ_ERR			(1 << 25)
#define TPU_BAD_ADDR			(1 << 24)
#define TPU_UNS_XOP			(1 << 23)
#define TPU_XREQ_OFLOW			(1 << 22)
#define TPU_LLP_REC_SN_ERR		(1 << 21)
#define TPU_LLP_REC_CB_ERR		(1 << 20)
#define TPU_LLP_RR_CNT_INT		(1 << 19)
#define TPU_LLP_TX_RETRY		(1 << 18)
#define TPU_LLP_TR_CNT_INT		(1 << 17)
#define TPU_XLATE_ERR			(1 << 14)
#define TPU_DMA1_ERR			(1 << 13)
#define TPU_DMA0_ERR			(1 << 12)
#define TPU_XRD_REQ_TO			(1 << 9)
#define TPU_XWR_REQ_TO			(1 << 8)
#define TPU_AUX_INT_6			(1 << 6)
#define TPU_AUX_INT_5			(1 << 5)
#define TPU_AUX_INT_4			(1 << 4)
#define TPU_AUX_INT_3			(1 << 3)
#define TPU_AUX_INT_2			(1 << 2)
#define TPU_AUX_INT_1			(1 << 1)
#define TPU_AUX_INT_0			(1 << 0)
#define TPU_INT_STATUS_REG_MSK		(TPU_LINK_RESET	    | \
					 TPU_UNEXP_RESP	    | \
					 TPU_BAD_XRSP_PKT   | \
					 TPU_BAD_XREQ_PKT   | \
					 TPU_RESP_ERR	    | \
					 TPU_REQ_ERR	    | \
					 TPU_BAD_ADDR	    | \
					 TPU_UNS_XOP	    | \
					 TPU_XREQ_OFLOW	    | \
					 TPU_LLP_REC_SN_ERR | \
					 TPU_LLP_REC_CB_ERR | \
					 TPU_LLP_RR_CNT_INT | \
					 TPU_LLP_TX_RETRY   | \
					 TPU_LLP_TR_CNT_INT | \
					 TPU_XLATE_ERR	    | \
					 TPU_DMA1_ERR	    | \
					 TPU_DMA0_ERR	    | \
					 TPU_XRD_REQ_TO	    | \
					 TPU_XWR_REQ_TO	    | \
					 TPU_AUX_INT_6	    | \
					 TPU_AUX_INT_5	    | \
					 TPU_AUX_INT_4	    | \
					 TPU_AUX_INT_3	    | \
					 TPU_AUX_INT_2	    | \
					 TPU_AUX_INT_1	    | \
					 TPU_AUX_INT_0	    )
#define TPU_INT_ENABLE_REG_MSK		(TPU_INT_STATUS_REG_MSK ^ \
					 (TPU_LINK_RESET	| \
					  TPU_AUX_INT_6		| \
					  TPU_AUX_INT_5		| \
					  TPU_AUX_INT_4		| \
					  TPU_AUX_INT_3		| \
					  TPU_AUX_INT_2		| \
					  TPU_AUX_INT_1		| \
					  TPU_AUX_INT_0		))
#define TPU_INT_STATUS_RESET_REG_MSK	TPU_INT_ENABLE_REG_MSK

/* 0x0_0134 -> 0x0_0164 Aux Interrupt 0-6 Addr reg */

#define TPU_AUX_INT_ADDR_SHFT		(8)
#define TPU_AUX_INT_ADDR_MSK		(0xffff << TPU_AUX_INT_ADDR_SHFT)
#define TPU_AUX_INT_VECTOR_SHFT		(0)
#define TPU_AUX_INT_VECTOR_MSK		(0xff << TPU_AUX_INT_VECTOR_SHFT)
#define TPU_AUX_INT(addr48,vect)	((TPU_AUX_INT_ADDR_MSK & ((addr48) >> 24)) | \
					 (((vect) << TPU_AUX_INT_VECTOR_SHFT) & TPU_AUX_INT_VECTOR_MSK))
#define TPU_AUX_INT_ADDR_REG_MSK	(TPU_AUX_INT_ADDR_MSK  | \
					 TPU_AUX_INT_VECTOR_MSK)

/* 0x0_0184 User Definable Sense reg */

#define TPU_SENSE_PIN_MSK		(0xff)
#define TPU_SENSE_PIN(bit)		(TPU_SENSE_PIN_MSK & (1 << bit))
#define TPU_SENSE_PIN_0			(1 << 0)
#define TPU_SENSE_PIN_1			(1 << 1)
#define TPU_SENSE_PIN_2			(1 << 2)
#define TPU_SENSE_PIN_3			(1 << 3)
#define TPU_SENSE_PIN_4			(1 << 4)
#define TPU_SENSE_PIN_5			(1 << 5)
#define TPU_SENSE_PIN_6			(1 << 6)
#define TPU_SENSE_PIN_7			(1 << 7)
#define TPU_SENSE_REG_MSK		TPU_SENSE_PIN_MSK

/* 0x0_018c User Definable Enable reg */

#define TPU_ENABLE_SELECT		(0xffffff00)
#define TPU_ENABLE_PIN_MSK		(0xff)
#define TPU_ENABLE_PIN(bit)		(TPU_ENABLE_PIN_MSK & (1ull << bit))
#define TPU_ENABLE_PIN_0		(1 << 0)
#define TPU_ENABLE_PIN_1		(1 << 1)
#define TPU_ENABLE_PIN_2		(1 << 2)
#define TPU_ENABLE_PIN_3		(1 << 3)
#define TPU_ENABLE_PIN_4		(1 << 4)
#define TPU_ENABLE_PIN_5		(1 << 5)
#define TPU_ENABLE_PIN_6		(1 << 6)
#define TPU_ENABLE_PIN_7		(1 << 7)
#define TPU_ENABLE_REG_MSK		(TPU_ENABLE_SELECT | \
					 TPU_ENABLE_PIN_MSK)

/*
 * Address translation table registers.
 */

/* 0x80_0000 Configuration register */

#define ATT_ENABLE_1			(1 << 5)
#define ATT_ENABLE_0			(1 << 4)
#define ATT_PAGE_SIZE_MSK		(0x7)
#define ATT_PAGE_SIZE_SHFT		(0x0)
#define ATT_PAGE_INCREMENT		(0x1)
#define ATT_PAGE_SIZE_16K		(0x0)
#define ATT_PAGE_SIZE_64K		(0x1)
#define ATT_PAGE_SIZE_256K		(0x2)
#define ATT_PAGE_SIZE_1M		(0x3)
#define ATT_PAGE_SIZE_4M		(0x4)
#define ATT_PAGE_SIZE_16M		(0x5)
#define ATT_PAGE_SIZE_64M		(0x6)
#define ATT_PAGE_SIZE_256M		(0x7)

/* 0x80_0008 Diagnostic mode register */

#define ATT_PARITY_FORCE		(1 << 4)

/* 0x80_0400 - 0x80_07f8 Table entries */

#define ATT_TABLE_PE_SHFT		(50)
#define ATT_TABLE_PE_MSK		(0xfull << ATT_TABLE_PE_SHFT)
#define ATT_TABLE_ENTRY_VALID		(0x1ull << 49)
#define ATT_TABLE_RO_ENTRY		(0x1ull << 48)
#define ATT_TABLE_PAGE_ADDR_SHFT	(0)
#define ATT_TABLE_PAGE_ADDR_MSK		(0xffffffffc000ull << ATT_TABLE_PAGE_ADDR_SHFT)


/*
 * DMA engine registers.
 */

/* 0x10_0010 Status register */

#define DEI_XLATE_OOB			(1 << 26)
#define DEI_XLATE_INV			(1 << 25)
#define DEI_XLATE_PE			(1 << 24)
#define DEI_PROT_ERR			(1 << 22)
#define DEI_WR_REQ_TO			(1 << 21)
#define DEI_RD_REQ_TO			(1 << 20)
#define DEI_PBUF_UFLOW			(1 << 19)
#define DEI_XPKT_ERR			(1 << 18)
#define DEI_PBUF_OFLOW			(1 << 17)
#define DEI_RESP_ERR			(1 << 16)
#define DEI_STALE_PKT			(1 << 13)
#define DEI_PPG_PE			(1 << 12)
#define DEI_PBUF_PE_SHFT		(8)
#define DEI_PBUF_PE			(0xf << DEI_PBUF_PE_SHFT)
#define DEI_CREDIT_CNT_SHFT		(4)
#define DEI_CREDIT_CNT			(0xf << DEI_CREDIT_CNT_SHFT)
#define DEI_REQ_OUT			(0xf)
#define DEI_STATUS_REG_MSK		(DEI_XLATE_OOB	| \
					 DEI_XLATE_INV	| \
					 DEI_XLATE_PE	| \
					 DEI_PROT_ERR	| \
					 DEI_WR_REQ_TO	| \
					 DEI_RD_REQ_TO	| \
					 DEI_PBUF_UFLOW | \
					 DEI_XPKT_ERR	| \
					 DEI_PBUF_OFLOW | \
					 DEI_RESP_ERR	| \
					 DEI_STALE_PKT	| \
					 DEI_PPG_PE	| \
					 DEI_PBUF_PE	| \
					 DEI_CREDIT_CNT | \
					 DEI_REQ_OUT	)
#define DEI_0_STATUS_REG_MSK		 DEI_STATUS_REG_MSK
#define DEI_1_STATUS_REG_MSK		 DEI_STATUS_REG_MSK


/*
 *	LDI registers.
 */

/* Register 0: l_HostInstruction */

#define LDI_X_RISC_I_OPCODE_MSW_MSK	0xffffffff00000000
#define LDI_X_RISC_I_OPCODE_MSW_SHFT	32
#define LDI_X_RISC_I_OPCODE_LSW_MSK	0x00000000ffffffff
#define LDI_X_RISC_I_OPCODE_LSW_SHFT	0
#define LDI_RISC_I_OPCODE_REG_MSK	(LDI_X_RISC_I_OPCODE_MSW_MSK | \

/* Register 1: l_sRiscMode */

#define LDI_S_RISC_MODE			(0x1ull << 63)
#define LDI_X_RISC_OPCODE		(0xfull << 59)
#define LDI_X_RISC_I_CODE		(0xfull << 55)
#define LDI_X_PORT_BIT			(  1ull << 54)
#define LDI_X_PORT_ONE			LDI_X_PORT_BIT
#define LDI_R_PORT_BIT			(  1ull << 53)
#define LDI_R_PORT_ONE			LDI_R_PORT_BIT
#define LDI_S_RESET_MSK			(  1ull << 52)
#define LDI_S_SEL_TP			(  3ull << 50)
#define LDI_X_RISC_B_SHFT		32
#  define LDI_BARRIER_R_DONE		(  1ull << 48)
#  define LDI_BARRIER_X_DONE		(  1ull << 47)
#  define LDI_BARRIER_SINGLE_CYCLE	(  1ull << 46)
#  define LDI_BARRIER_RISC		(  1ull << 45)
#  define LDI_BARRIER_INTERRUPT		(  1ull << 44)
#  define LDI_BARRIER_CLEAR_SELECT	(  1ull << 43)
#  define LDI_BARRIER_CND_SELECT	(  1ull << 42)
#  define LDI_BARRIER_LOGIC_OR		LDI_BARRIER_CND_SELECT
#  define LDI_BARRIER_LSP_PEND		(  1ull << 41)
#  define LDI_BARRIER_LSP		(  1ull << 40)
#  define LDI_BARRIER_STATUS_CODE_MSK	0x000000ff00000000
#define LDI_X_RISC_B_MSK		(LDI_BARRIER_R_DONE	  | \
					 LDI_BARRIER_X_DONE	  | \
					 LDI_BARRIER_SINGLE_CYCLE | \
					 LDI_BARRIER_RISC	  | \
					 LDI_BARRIER_INTERRUPT	  | \
					 LDI_BARRIER_CLEAR_SELECT | \
					 LDI_BARRIER_CND_SELECT	  | \
					 LDI_BARRIER_LSP_PEND	  | \
					 LDI_BARRIER_LSP	  | \
					 LDI_BARRIER_STATUS_CODE_MSK)
#define LDI_S_C_DONE_CODE_SHFT		(24)
#define LDI_S_C_DONE_CODE_MSK		(0xffull << LDI_S_C_DONE_CODE_SHFT)
#define LDI_X_RISC_BLK_SIZE_SHFT	(0)
#define LDI_X_RISC_BLK_SIZE_MSK		(0x3fffffull << LDI_X_RISC_BLK_SIZE_SHFT)
#define LDI_RISC_MODE_REG_MSK		(LDI_S_RISC_MODE       | \
					 LDI_X_RISC_OPCODE     | \
					 LDI_X_RISC_I_CODE     | \
					 LDI_X_PORT_BIT	       | \
					 LDI_R_PORT_BIT	       | \
					 LDI_S_RESET_MSK       | \
					 LDI_S_SEL_TP	       | \
					 LDI_X_RISC_B_MSK      | \
					 LDI_S_C_DONE_CODE_MSK | \
					 LDI_X_RISC_BLK_SIZE_MSK)

/* Register 2: l_xRiscSadr */

#define LDI_X_RISC_S_ADR_MSK		0x1fffffff00000000
#define LDI_X_RISC_S_ADR_SHFT		32
#define LDI_S_PAR_FORCE_LSP		(   1ull << 31) 
#define LDI_S_PAR_FORCE_I		(   1ull << 30) 
#define LDI_S_READ_BACK			(   1ull << 29) 
#define LDI_S_LOOP_BACK			(   1ull << 28) 
#define LDI_S_IEE			(   1ull << 20)
#define LDI_S_PAR_FORCE_X		(0x0full << 16) 
#define LDI_S_X_PORT_OFF_MSK		0x000000000000ffff
#define LDI_S_X_PORT_OFF_SHFT		0
#define LDI_X_RISC_S_ADR_REG_MSK	(LDI_X_RISC_S_ADR_MSK | \
					 LDI_S_PAR_FORCE_LSP  | \
					 LDI_S_PAR_FORCE_I    | \
					 LDI_S_READ_BACK      | \
					 LDI_S_LOOP_BACK      | \
					 LDI_S_IEE	      | \
					 LDI_S_PAR_FORCE_X    | \
					 LDI_S_X_PORT_OFF_MSK)

/* Register 3: l_sCoefBs */

#define LDI_S_BK_DMP_CODE_MSK		0xff00000000000000 
#define LDI_S_COEF_BS_2_MSK		0x003fffff00000000 
#define LDI_S_JUMP_CODE_7_MSK		0x000000003fc00000 
#define LDI_S_COEF_BS_1_MSK		0x00000000003fffff 
#define LDI_COEF_BS_REG_MSK		(LDI_S_BK_DMP_CODE_MSK | \
					 LDI_S_COEF_BS_2_MSK   | \
					 LDI_S_JUMP_CODE_7_MSK | \
					 LDI_S_COEF_BS_1_MSK)

/* Register 4: l_sCoefInc */

#define LDI_S_COEF_INC_2_MSK		0x1fffffff00000000
#define LDI_S_COEF_INC_1_MSK		0x000000001fffffff
#define LDI_COEF_INC_REG_MSK		(LDI_S_COEF_INC_2_MSK | \
					 LDI_S_COEF_INC_1_MSK)

/* Register 5: l_sCoefInit */

#define LDI_S_COEF_INIT_2_MSK		0x1fffffff00000000
#define LDI_S_COEF_INIT_1_MSK		0x000000001fffffff
#define LDI_COEF_INIT_REG_MSK		(LDI_S_COEF_INIT_2_MSK | \
					 LDI_S_COEF_INIT_1_MSK)

/* Register 6: l_sDataBs */

#define LDI_S_X_PEN_CODE		0xff00000000000000
#define LDI_S_DATA_BS_2_MSK		0x003fffff00000000 
#define LDI_S_DATA_BS_1_MSK		0x00000000003fffff 
#define LDI_DATA_BS_REG_MSK		(LDI_S_X_PEN_CODE    | \
					 LDI_S_DATA_BS_2_MSK | \
					 LDI_S_DATA_BS_1_MSK)

/* Register 7: l_sSaBlkInc */

#define LDI_S_SA_BLK_INC_2_MSK		0x1fffffff00000000
#define LDI_S_SA_BLK_INC_1_MSK		0x000000001fffffff
#define LDI_DATA_INC_REG_MSK		(LDI_S_SA_BLK_INC_2_MSK | \
					 LDI_S_SA_BLK_INC_1_MSK)

/* Register 8: l_sSaBlkSize */

#define LDI_S_SA_BLK_SIZE_2_MSK		0x1fffffff00000000
#define LDI_S_SA_BLK_SIZE_1_MSK		0x000000001fffffff
#define LDI_SA_BLK_SIZE_REG_MSK		(LDI_S_SA_BLK_SIZE_2_MSK | \
					 LDI_S_SA_BLK_SIZE_1_MSK)

/* Register 9: l_sSaBlkInit */

#define LDI_S_SA_BLK_INIT_2_MSK		0x1fffffff00000000
#define LDI_S_SA_BLK_INIT_1_MSK		0x000000001fffffff
#define LDI_SA_BLK_INIT_REG_MSK		(LDI_S_SA_BLK_INIT_2_MSK | \
					 LDI_S_SA_BLK_INIT_1_MSK)

/* Register 10: l_tStatusReg */

/* Register 11: l_rSampInit */

#define LDI_R_SAMP_INIT_2_MSK		0x1fffffff00000000
#define LDI_R_SAMP_INIT_1_MSK		0x000000001fffffff
#define LDI_SAMP_INIT_REG_MSK		(LDI_R_SAMP_INIT_2_MSK | \
					 LDI_R_SAMP_INIT_1_MSK)

/* Register 12: l_rVecInitInc */

#define LDI_R_VECT_INIT_INC_2_MSK	0x1fffffff00000000
#define LDI_R_VECT_INIT_INC_1_MSK	0x000000001fffffff
#define LDI_VECT_INIT_INC_REG_MSK	(LDI_R_VECT_INIT_INC_2_MSK | \
					 LDI_R_VECT_INIT_INC_1_MSK)

/* Register 13: l_rVecInit */

#define LDI_R_VECT_INIT_2_MSK		0x1fffffff00000000
#define LDI_R_VECT_INIT_1_MSK		0x000000001fffffff
#define LDI_VECT_INIT_REG_MSK		(LDI_R_VECT_INIT_2_MSK | \
					 LDI_R_VECT_INIT_1_MSK)

/* Register 14: l_rVecInc */

#define LDI_R_VECT_INC_2_MSK		0x1fffffff00000000
#define LDI_R_VECT_INC_1_MSK		0x000000001fffffff
#define LDI_VECT_INC_REG_MSK		(LDI_R_VECT_INC_2_MSK | \
					 LDI_R_VECT_INC_1_MSK)

/* Register 15: l_sErrorMask */

#define LDI_DIAG_SET			(   1ull << 63)
#define LDI_WRT_ERR_INST		(   1ull << 60)
#define LDI_R_WARNING			(   1ull << 59)
#define LDI_X_WARNING			(   1ull << 58)
#define LDI_X_FIFO_OVERFLOW		(   1ull << 57)
#define LDI_X_FIFO_UNDERFLOW		(   1ull << 56)
#define LDI_R_FIFO_OVERFLOW		(   1ull << 55)
#define LDI_R_FIFO_UNDERFLOW		(   1ull << 54)
#define LDI_PC_OVERFLOW			(   1ull << 53)
#define LDI_XMIT_PE_SHIFT		(49)
#define LDI_XMIT_PE			(0x0full << LDI_XMIT_PE_SHIFT)
#define LDI_LSP_IN_PE			(   1ull << 48)
#define LDI_LSP_TO_LDI_PE		LDI_LSP_IN_PE
#define LDI_RD_ERR_INST			(   1ull << 47)
#define LDI_INST_PE			(   1ull << 46)
#define LDI_R_ERR			(   1ull << 45)
#define LDI_X_ERR			(   1ull << 44)
#define LDI_FORCE_PE			(   1ull << 43)
#define LDI_DISABLE_PARITY		(   1ull << 42)
#define LDI_CPF_DONE			(   1ull << 41)
#define LDI_BULK_MEM_DUMP_READY		(   1ull << 40)
#define LDI_INPUT_OPCODE_ERR		(   1ull << 39)
#define LDI_INPUT_IEEE_ERR		(   1ull << 38)
#define LDI_INPUT_PE			(   1ull << 37)
#define LDI_TO_LSP_PE			LDI_INPUT_PE
#define LDI_U_MEM_ODD_PE		(   1ull << 36)
#define LDI_MEM_A_UPPER_PE		(   1ull << 35)
#define LDI_MEM_A_LOWER_PE		(   1ull << 34)
#define LDI_MEM_A_PE_MSK		(LDI_MEM_A_UPPER_PE | LDI_MEM_A_LOWER_PE)
#define LDI_MEM_B_UPPER_PE		(   1ull << 33)
#define LDI_MEM_B_LOWER_PE		(   1ull << 32)
#define LDI_MEM_B_PE_MSK		(LDI_MEM_B_UPPER_PE | LDI_MEM_B_LOWER_PE)
#define LDI_MEM_C_UPPER_PE		(   1ull << 31)
#define LDI_MEM_C_LOWER_PE		(   1ull << 30)
#define LDI_MEM_C_PE_MSK		(LDI_MEM_C_UPPER_PE | LDI_MEM_C_LOWER_PE)
#define LDI_MEM_D_UPPER_PE		(   1ull << 29)
#define LDI_MEM_D_LOWER_PE		(   1ull << 28)
#define LDI_MEM_D_PE_MSK		(LDI_MEM_D_UPPER_PE | LDI_MEM_D_LOWER_PE)
#define LDI_MEM_E_UPPER_PE		(   1ull << 27)
#define LDI_MEM_E_LOWER_PE		(   1ull << 26)
#define LDI_MEM_E_PE_MSK		(LDI_MEM_E_UPPER_PE | LDI_MEM_E_LOWER_PE)
#define LDI_MEM_F_UPPER_PE		(   1ull << 25)
#define LDI_MEM_F_LOWER_PE		(   1ull << 24)
#define LDI_MEM_F_PE_MSK		(LDI_MEM_F_UPPER_PE | LDI_MEM_F_LOWER_PE)
#define LDI_MEM_G_UPPER_PE		(   1ull << 23)
#define LDI_MEM_G_LOWER_PE		(   1ull << 22)
#define LDI_MEM_G_PE_MSK		(LDI_MEM_G_UPPER_PE | LDI_MEM_G_LOWER_PE)
#define LDI_MEM_H_UPPER_PE		(   1ull << 21)
#define LDI_MEM_H_LOWER_PE		(   1ull << 20)
#define LDI_MEM_H_PE_MSK		(LDI_MEM_H_UPPER_PE | LDI_MEM_H_LOWER_PE)
#define LDI_MEM_PE(mem)			(LDI_MEM_A_PE_MSK << (((mem) - 0xa) * 2))
#define LDI_MEM_PE_MSK			(LDI_MEM_A_PE_MSK | LDI_MEM_B_PE_MSK | \
					 LDI_MEM_C_PE_MSK | LDI_MEM_D_PE_MSK | \
					 LDI_MEM_E_PE_MSK | LDI_MEM_F_PE_MSK | \
					 LDI_MEM_G_PE_MSK | LDI_MEM_H_PE_MSK)
#define LDI_FP_A_OVERFLOW_ERR		(   7ull << 17)
#define LDI_FP_B_OVERFLOW_ERR		(   7ull << 14)
#define LDI_FX_ADD_OVERFLOW_ERR		(   1ull << 13)
#define LDI_PORT_1_OVERFLOW_ERR		(   1ull << 12)
#define LDI_PORT_2_OVERFLOW_ERR		(   1ull << 11)
#define LDI_SM_STATUS			0x00000000000007ff
#define LDI_STATUS_REG_MSK		(LDI_DIAG_SET		 | \
					 LDI_WRT_ERR_INST	 | \
					 LDI_R_WARNING		 | \
					 LDI_X_WARNING		 | \
					 LDI_X_FIFO_OVERFLOW	 | \
					 LDI_X_FIFO_UNDERFLOW	 | \
					 LDI_R_FIFO_OVERFLOW	 | \
					 LDI_R_FIFO_UNDERFLOW	 | \
					 LDI_PC_OVERFLOW	 | \
					 LDI_XMIT_PE		 | \
					 LDI_LSP_IN_PE		 | \
					 LDI_RD_ERR_INST	 | \
					 LDI_INST_PE		 | \
					 LDI_R_ERR		 | \
					 LDI_X_ERR		 | \
					 LDI_FORCE_PE		 | \
					 LDI_DISABLE_PARITY	 | \
					 LDI_CPF_DONE		 | \
					 LDI_BULK_MEM_DUMP_READY | \
					 LDI_INPUT_OPCODE_ERR	 | \
					 LDI_INPUT_IEEE_ERR	 | \
					 LDI_INPUT_PE		 | \
					 LDI_U_MEM_ODD_PE	 | \
					 LDI_MEM_PE_MSK		 | \
					 LDI_FP_A_OVERFLOW_ERR	 | \
					 LDI_FP_B_OVERFLOW_ERR	 | \
					 LDI_FX_ADD_OVERFLOW_ERR | \
					 LDI_PORT_1_OVERFLOW_ERR | \
					 LDI_PORT_2_OVERFLOW_ERR | \
					 LDI_SM_STATUS)
#define LDI_ERR_MASK_REG_MSK		(LDI_STATUS_REG_MSK & ~LDI_DIAG_SET)
#define LDI_STATUS_REG_REG_MSK		 LDI_STATUS_REG_MSK

/* Register 16: l_sLBABlkSize */

#define LDI_S_LBA_BLK_SIZE_MSK		(0xffffull << 48)
#define LDI_S_J_3_MSK			(0x00ffull << 40)
#define LDI_S_J_2_MSK			(0x00ffull << 32)
#define LDI_S_J_1_MSK			(0x00ffull << 24)
#define LDI_S_J_0_MSK			(0x00ffull << 16)
#define LDI_S_JUMP_CODE_3_MSK		(0x00ffull << 8)
#define LDI_S_JUMP_CODE_2_MSK		(0x00ffull << 0)
#define LDI_LBA_BLK_SIZE_REG_MSK	(LDI_S_LBA_BLK_SIZE_MSK | \
					 LDI_S_J_3_MSK		| \
					 LDI_S_J_2_MSK		| \
					 LDI_S_J_1_MSK		| \
					 LDI_S_J_0_MSK		| \
					 LDI_S_JUMP_CODE_3_MSK	| \
					 LDI_S_JUMP_CODE_2_MSK)

/* Register 17: l_sLoopCnt */

#define LDI_S_LOOP_CNT_MSK		0xffffffff00000000
#define LDI_S_LOOP_SEL_1		(     1ull << 31)
#define LDI_S_LOOP_SEL_2		(     1ull << 30)
#define LDI_S_MANUAL			(     1ull << 28)
#define LDI_S_JUMP_CODE_1_MSK		(0x00ffull << 20)
#define LDI_S_JUMP_CODE_0_MSK		(0x00ffull << 12)
#define LDI_S_B_BRANCH_ADDR_0_MSK	(0x003full << 6)
#define LDI_S_A_BRANCH_ADDR_0_MSK	(0x003full << 0)
#define LDI_LOOP_CNT_REG_MSK		(LDI_S_LOOP_CNT_MSK	   | \
					 LDI_S_LOOP_SEL_1	   | \
					 LDI_S_LOOP_SEL_2	   | \
					 LDI_S_MANUAL		   | \
					 LDI_S_JUMP_CODE_1_MSK	   | \
					 LDI_S_JUMP_CODE_0_MSK	   | \
					 LDI_S_B_BRANCH_ADDR_0_MSK | \
					 LDI_S_A_BRANCH_ADDR_0_MSK)

/* Register 18: l_tDiagReg */

#define LDI_T_DIAG_MSK			(0xffffffffffffffff)
#define LDI_DIAG_REG_MSK		LDI_T_DIAG_MSK
#define LDI_DIAG_REG_REG_MSK		LDI_DIAG_REG_MSK

/* Register 19: l_tTimerReg */

#define LDI_T_TIMER_SHFT		32
#define LDI_T_TIMER_MSK			(0xffffffffull << LDI_T_TIMER_SHFT)
#define LDI_IBAR_STAT			(0x3full << 25) 
#define LDI_T_BARRIER_PEND_HOLD_SRC	(0x01ull << 24)
#define LDI_T_BARRIER_IBAR_WAIT_SRC	(0x01ull << 23)
#define LDI_T_BARRIER_LSP_BARRIER_SRC	(0x01ull << 22)
#define LDI_T_BARRIER_PEND_SRC		(0x01ull << 21)
#define LDI_T_BARRIER_RISC_SRC		(0x01ull << 20)
#define LDI_T_BARRIER_SINGLE_CYCLE_SRC (0x01ull << 19)
#define LDI_T_BARRIER_X_DONE_SRC	(0x01ull << 18)
#define LDI_T_BARRIER_R_DONE_SRC	(0x01ull << 17)
#define LDI_T_BARRIER_R_DONE		(LDI_BARRIER_R_DONE >> 32)
#define LDI_T_BARRIER_X_DONE		(LDI_BARRIER_X_DONE >> 32)
#define LDI_T_BARRIER_SINGLE_CYCLE	(LDI_BARRIER_SINGLE_CYCLE >> 32)
#define LDI_T_BARRIER_RISC		(LDI_BARRIER_RISC >> 32)
#define LDI_T_BARRIER_INTERRUPT		(LDI_BARRIER_INTERRUPT >> 32)
#define LDI_T_BARRIER_CLEAR_SELECT	(LDI_BARRIER_CLEAR_SELECT >> 32)
#define LDI_T_BARRIER_CND_SELECT	(LDI_BARRIER_CND_SELECT >> 32)
#define LDI_T_BARRIER_LSP		(LDI_BARRIER_LSP >> 32)
#define LDI_T_BARRIER_STATUS_CODE_MSK	(LDI_BARRIER_STATUS_CODE_MSK >> 32)
#define LDI_T_BARRIER_REG_MSK		(LDI_T_BARRIER_PEND_HOLD_SRC	| \
					 LDI_T_BARRIER_IBAR_WAIT_SRC	| \
					 LDI_T_BARRIER_LSP_BARRIER_SRC	| \
					 LDI_T_BARRIER_PEND_SRC		| \
					 LDI_T_BARRIER_RISC_SRC		| \
					 LDI_T_BARRIER_SINGLE_CYCLE_SRC | \
					 LDI_T_BARRIER_X_DONE_SRC	| \
					 LDI_T_BARRIER_R_DONE_SRC	| \
					 LDI_T_BARRIER_R_DONE		| \
					 LDI_T_BARRIER_X_DONE		| \
					 LDI_T_BARRIER_SINGLE_CYCLE	| \
					 LDI_T_BARRIER_RISC		| \
					 LDI_T_BARRIER_INTERRUPT	| \
					 LDI_T_BARRIER_CLEAR_SELECT	| \
					 LDI_T_BARRIER_CND_SELECT	| \
					 LDI_T_BARRIER_LSP		| \
					 LDI_T_BARRIER_STATUS_CODE_MSK)
#define LDI_I_SOURCE_REG_MSK		(LDI_T_TIMER_MSK | LDI_T_BARRIER_REG_MSK)
#define LDI_TIMER_REG_MSK		LDI_I_SOURCE_REG_MSK
#define LDI_TIMER_REG_REG_MSK		LDI_TIMER_REG_MSK

/* Register 20-27: l_xPipeInBus[8] */

#define LDI_PIPE_IN_STRT_ADDR_SHFT	(32)
#define LDI_PIPE_IN_STRT_ADDR_MSK	(0x1fffffffull << LDI_PIPE_IN_STRT_ADDR_SHFT)
#define LDI_PIPE_IN_BLK_SIZE_SHFT	(0)
#define LDI_PIPE_IN_BLK_SIZE_MSK	(0x3fffffull << LDI_PIPE_IN_BLK_SIZE_SHFT)
#define LDI_PIPE_IN_BUS_REG_MSK		(LDI_PIPE_IN_STRT_ADDR_MSK | \
					 LDI_PIPE_IN_BLK_SIZE_MSK)
#define LDI_PIPE_IN_BUS_REG(adr, blk)	(((((ULL)(adr) >> 3) << LDI_PIPE_IN_STRT_ADDR_SHFT) | \
					 ((blk) << LDI_PIPE_IN_BLK_SIZE_SHFT)) & \
					 LDI_PIPE_IN_BUS_REG_MSK)

/* Register 28: l_sRcvBlkSize */

#define LDI_S_C_DONE_MSK		0xff00000000000000
#define LDI_S_BK_DMP_MSK		0x00ff000000000000
#define LDI_S_RCV_BLK_SIZE_MSK		0x0000ffff00000000
#define LDI_R_RISC_S_ADR_MSK		0x000000001fffffff
#define LDI_R_RISC_S_ADR_SHFT		0
#define LDI_RCV_BLK_SIZE_REG_MSK	(LDI_S_C_DONE_MSK	| \
					 LDI_S_BK_DMP_MSK	| \
					 LDI_S_RCV_BLK_SIZE_MSK | \
					 LDI_R_RISC_S_ADR_MSK)

/* Register 29: l_rSampInc */

#define LDI_R_SAMP_INC_2_MSK		0x1fffffff00000000
#define LDI_R_SAMP_INC_1_MSK		0x000000001fffffff
#define LDI_SAMP_INC_REG_MSK		(LDI_R_SAMP_INC_2_MSK | \
					 LDI_R_SAMP_INC_1_MSK)

/* Register 30: l_rSampInc */

#define LDI_R_O_LINE_SIZE_2_MSK		0x1fffffff00000000
#define LDI_R_O_LINE_SIZE_1_MSK		0x000000001fffffff
#define LDI_O_LINE_SIZE_REG_MSK		(LDI_R_O_LINE_SIZE_2_MSK | \
					 LDI_R_O_LINE_SIZE_1_MSK)

/* Register 31: l_srPortOff */

#define LDI_S_R_PORT_OFF_MSK		(0xffffull << 48)
#define LDI_S_R_PORT_OFF_SHFT		48
#define LDI_S_ANGLE_MEM			(0x1ull << 47)
#define LDI_S_PAR_FORCE_R		(0xfull << 43)
#define LDI_S_R_WRN_MSK			(0x1ull << 42)
#define LDI_S_X_WRN_MSK			(0x1ull << 41)
#define LDI_S_HOST_BARRIER_PEND		(0x1ull << 39)
#define LDI_S_HOST_BARRIER_DISABLE	(0x1ull << 38)
#define LDI_S_HOST_BARRIER_LOGIC_CND	(0x1ull << 37)
#define LDI_S_HOST_BARRIER_LOGIC_OR	LDI_S_HOST_BARRIER_LOGIC_CND
#define LDI_S_HOST_BARRIER_IBAR_WAIT	(0x1ull << 36)
#define LDI_S_HOST_BARRIER_LSP_BARRIER	(0x1ull << 35)
#define LDI_S_HOST_BARRIER_SINGLE_CYCLE (0x1ull << 34)
#define LDI_S_HOST_BARRIER_X_DONE	(0x1ull << 33)
#define LDI_S_HOST_BARRIER_R_DONE	(0x1ull << 32)
#define LDI_S_ADD_LINE_COUNT_SHFT	(0)
#define LDI_S_ADD_LINE_COUNT_MSK	(0x1fffffffull << LDI_S_ADD_LINE_COUNT_SHFT)
#define LDI_S_HOST_BARRIER_INT_MSK	(LDI_S_HOST_BARRIER_PEND	 | \
					 LDI_S_HOST_BARRIER_DISABLE	 | \
					 LDI_S_HOST_BARRIER_LOGIC_CND	 | \
					 LDI_S_HOST_BARRIER_IBAR_WAIT	 | \
					 LDI_S_HOST_BARRIER_LSP_BARRIER	 | \
					 LDI_S_HOST_BARRIER_SINGLE_CYCLE | \
					 LDI_S_HOST_BARRIER_X_DONE	 | \
					 LDI_S_HOST_BARRIER_R_DONE	   )
#define LDI_I_MASK_REG_MSK		(LDI_S_R_PORT_OFF_MSK	    | \
					 LDI_S_ANGLE_MEM	    | \
					 LDI_S_PAR_FORCE_R	    | \
					 LDI_S_R_WRN_MSK	    | \
					 LDI_S_X_WRN_MSK	    | \
					 LDI_S_ADD_LINE_COUNT_MSK   | \
					 LDI_S_HOST_BARRIER_INT_MSK)

/* Register 32: l_tFlag */
/* Register 33: l_bFlag */

#define LDI_LSP_BP_RST			(1ull << 15)
#define LDI_JMP_RST			(1ull << 14)
#define LDI_C_ERR_STAT_CLEAR		(1ull << 13)
#define LDI_R_SYS_GO			(1ull << 12)
#define LDI_RISC_BARRIER_SET		(1ull << 11)
#define LDI_CHAIN_GO			(1ull << 10)
#define LDI_BOOT			(1ull << 9)
#define LDI_P_OVERRIDE			(1ull << 8)
#define LDI_DIAG_STAT_CLEAR		(1ull << 7)
#define LDI_ERR_STAT_CLEAR		(1ull << 6)
#define LDI_SYS_GO			(1ull << 5)
#define LDI_ADVANCE_ADR			(1ull << 4)
#define LDI_B_INTERRUPT_CLEAR		(1ull << 3)
#define LDI_P_RESET			(1ull << 2)
#define LDI_TIMER_STAT_CLEAR		(1ull << 1)
#define LDI_TIMER_START			(1ull << 0)
#define LDI_FLAG_REG_MSK		(LDI_LSP_BP_RST	       | \
					 LDI_JMP_RST	       | \
					 LDI_C_ERR_STAT_CLEAR  | \
					 LDI_R_SYS_GO	       | \
					 LDI_RISC_BARRIER_SET  | \
					 LDI_CHAIN_GO	       | \
					 LDI_BOOT	       | \
					 LDI_P_OVERRIDE	       | \
					 LDI_DIAG_STAT_CLEAR   | \
					 LDI_ERR_STAT_CLEAR    | \
					 LDI_SYS_GO	       | \
					 LDI_ADVANCE_ADR       | \
					 LDI_B_INTERRUPT_CLEAR | \
					 LDI_P_RESET	       | \
					 LDI_TIMER_STAT_CLEAR  | \
					 LDI_TIMER_START)
#define LDI_T_FLAG_REG_MSK		LDI_FLAG_REG_MSK
#define LDI_B_FLAG_REG_MSK		LDI_FLAG_REG_MSK

/* Register 34: l_DiagCompReg */

#define LDI_S_DIAG_PART_MSK		0xffffffffffffffff
#define LDI_DIAG_PART_REG_MSK		LDI_S_DIAG_PART_MSK

/* Register 35: l_JumpMask */

#define LDI_S_J_7_MSK			(0xffull << 56)
#define LDI_S_J_6_MSK			(0xffull << 48)
#define LDI_S_J_5_MSK			(0xffull << 40)
#define LDI_S_J_4_MSK			(0xffull << 32)
#define LDI_S_JUMP_CODE_6_MSK		(0xffull << 24)
#define LDI_S_JUMP_CODE_5_MSK		(0xffull << 16)
#define LDI_S_JUMP_CODE_4_MSK		(0xffull << 8)
#define LDI_S_TIMER_CODE_MSK		(0xffull << 0)
#define LDI_JMP_CND_REG_MSK		(LDI_S_J_7_MSK	       | \
					 LDI_S_J_6_MSK	       | \
					 LDI_S_J_5_MSK	       | \
					 LDI_S_J_4_MSK	       | \
					 LDI_S_JUMP_CODE_6_MSK | \
					 LDI_S_JUMP_CODE_5_MSK | \
					 LDI_S_JUMP_CODE_4_MSK | \
					 LDI_S_TIMER_CODE_MSK)

/* LDI instruction memory */

#define LDI_INST_UPPER_REG_MSK		0x000000007fffffff
#define LDI_INST_LOWER_REG_MSK		0xffffffffffffffff

/* LDI instruction word */

#define LDI_I_SIZE			64
#define LDI_I_UPPER			0
#define LDI_I_LOWER			1
#define LDI_I_BASE			0x300400

/* LDI instruction Icode values */

#define LDI_ID_OPCODE			(0x0)
#define LDI_ID_BLOCK			(0x1)
#define LDI_ID_CPF			(0x2)
#define LDI_ID_JUMP2			(0x3)
#define LDI_ID_JUMP3			(0x4)
#define LDI_ID_L_SREG			(0x5)
#define LDI_ID_DMA			(0x6)
#define LDI_ID_MIS_CLK			(0x7)
#define LDI_ID_PIPE_CLK			LDI_ID_MIS_CLK
#define LDI_ID_DMA_RESET		(0x8)
#define LDI_ID_NOP			(0x9)
#define LDI_ID_BARRIER_WAIT		(0xa)
#define LDI_ID_C_WRITE			(0xb)
#define LDI_ID_I_DMA_CTR		(0xc)
#define LDI_ID_X_SEL			(0xd)
#define LDI_ID_R_SEL			(0xe)
#define LDI_ID_TST			(0xf)

/* LDI instruction XSEL field */

#define LDI_IX_PIPE			(0x0)
#define LDI_IX_COEF			(0x1)
#define LDI_IX_DATA			(0x2)
#define LDI_IX_INST			(0x3)

/* LDI instruction mask field */

#define LDI_IM_R_DONE			(1ull << 16)
#define LDI_IM_X_DONE			(1ull << 15)
#define LDI_IM_SINGLE_CYCLE		(1ull << 14)
#define LDI_IM_RISC			(1ull << 13)
#define LDI_IM_B_INTERRUPT		(1ull << 12)
#define LDI_IM_CLEAR_SELECT		(1ull << 11)
#define LDI_IM_CND_SELECT		(1ull << 10)
#define LDI_IM_LSP_ENABLE		(1ull << 8)
#define LDI_IM_LSP_STATUS_MSK		(0xff << 0)

/* LSP I/O instructions */

#define LSP_IO_NOP			(0x0)
#define LSP_IO_LCA			(0x1)
#define LSP_IO_LSA			(0x2)
#define LSP_IO_LBA			(0x3)
#define LSP_IO_PAR			(0x4)
#define LSP_IO_LRA			(0x5)
#define LSP_IO_LMA			(0x7)
#define LSP_IO_SCA			(0x8)
#define LSP_IO_SBA			(0x9)
#define LSP_IO_SMA			(0xA)
#define LSP_IO_SRA			(0xB)
#define LSP_IO_SST			(0xF)

/* LDI instruction opcode values */

#define LDI_IO_NOP			LSP_IO_NOP
#define LDI_IO_LCA			LSP_IO_LCA
#define LDI_IO_LSA			LSP_IO_LSA
#define LDI_IO_LBA			LSP_IO_LBA
#define LDI_IO_PAR			LSP_IO_PAR
#define LDI_IO_LRA			LSP_IO_LRA
#define LDI_IO_LMA			LSP_IO_LMA
#define LDI_IO_SCA			LSP_IO_SCA
#define LDI_IO_SBA			LSP_IO_SBA
#define LDI_IO_SMA			LSP_IO_SMA
#define LDI_IO_SRA			LSP_IO_SRA
#define LDI_IO_SST			LSP_IO_SST

/* Upper LDI instruction word */

#define LDI_I_CODE_SHFT			(91 - 64)
#define LDI_I_XSEL_SHFT			(89 - 64)
#define LDI_I_MASK_SHFT			(72 - 64)
#define LDI_I_OPCODE_SHFT		(64 - 64)
#define LDI_I_REG_ADDR_SHFT		(64 - 64)
#define LDI_I_CODE(icode)		(((icode)  & 0x0000full) << LDI_I_CODE_SHFT)
#define LDI_I_XSEL(xsel)		(((xsel)   & 0x00003ull) << LDI_I_XSEL_SHFT)
#define LDI_I_MASK(mask)		(((mask)   & 0x1ffffull) << LDI_I_MASK_SHFT)
#define LDI_I_OPCODE(opcode)		(((opcode) & 0x0000full) << LDI_I_OPCODE_SHFT)
#define LDI_I_REG_ADDR(addr)		(((addr)   & 0x000ffull) << LDI_I_REG_ADDR_SHFT)

/* Lower LDI instruction word */

#define LDI_I_HOST_ADDR_SHIFT		(32)
#define LDI_I_HOST_ADDR_MSK		(0xffffffffull << LDI_I_HOST_ADDR_SHIFT)
#define LDI_I_BLOCK_SHIFT		(0)
#define LDI_I_BLOCK_MSK			(0xffffffffull << LDI_I_BLOCK_SHIFT)
#define LDI_I_HOST_ADDR(bAddr)		((((bAddr) >> 3) << LDI_I_HOST_ADDR_SHIFT) & LDI_I_HOST_ADDR_MSK)
#define LDI_I_BLOCK(block)		(((block) << LDI_I_BLOCK_SHIFT) & LDI_I_BLOCK_MSK)

/* Common LDI instructions */

#define LDI_I_IBAR_WAIT_UPPER		(LDI_I_CODE (LDI_ID_BARRIER_WAIT) | \
					 LDI_I_XSEL (0)			  | \
					 LDI_I_MASK (LDI_IM_B_INTERRUPT | LDI_IM_RISC)	)
#define LDI_I_IBAR_WAIT_LOWER		(0x0000000000000000ULL)

#define LDI_I_DMA_RESET_UPPER		(LDI_I_CODE	(LDI_ID_DMA_RESET) | \
					 LDI_I_XSEL	(LDI_IX_INST)	   | \
					 LDI_I_MASK	(LDI_IM_X_DONE)	   | \
					 LDI_I_REG_ADDR (92+35)	   )
#define LDI_I_DMA_RESET_LOWER		(0x0000000000000000ULL)


#ifdef __cplusplus
}
#endif

#endif /* __SYS_TPUREG_H__ */
