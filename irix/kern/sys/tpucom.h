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
 * sys/tpucom.h - Mesa TPU common driver/library definitions.			*
 *										*
 ********************************************************************************/

#ifndef __SYS_TPUCOM_H__
#define __SYS_TPUCOM_H__

#ifdef __cplusplus
extern "C" {
#endif

#ident  "$Id: tpucom.h,v 1.4 1999/04/29 19:32:28 pww Exp $"

#define TPU_DEVS_MAX	288	/* max TPUs in a system */
#define	TPU_PAGES_MAX	128	/* max translation table entries */
#define	TPU_NAME_MAX	32	/* max length of short device name */

#define	TPU_MAGIC	(('T' << 24) | ('P' << 16) | ('U' << 8))
#define	TPUDBG_MAGIC	(('T' << 24) | ('P' << 16) | ('D' << 8))
#define	TPUSIM_MAGIC	(('T' << 24) | ('P' << 16) | ('S' << 8))
#define	LIBTPU_MAGIC	(('T' << 24) | ('P' << 16) | ('L' << 8))

/*
 * Structure magic numbers.
 */
#define	TPU_M_SOFT	(TPU_MAGIC | 's')
#define	TPU_M_TRACE	(TPU_MAGIC | 'u')
#define	TPU_M_TRACEX	(TPU_MAGIC | 'U')

/*
 * tpusim ioctl() function codes.
 */
#define	TPUSIM_ATTACH	(TPUSIM_MAGIC |  1)

/*
 * tpu ioctl() function codes.
 */
#define	TPU_RUN		(TPU_MAGIC |  8)
#define	TPU_RESUME	(TPU_MAGIC |  3)
#define	TPU_HALT	(TPU_MAGIC |  2)
#define	TPU_REGS	(TPU_MAGIC |  4)
#define	TPU_INST	(TPU_MAGIC |  6)
#define	TPU_STAT	(TPU_MAGIC | 13) 
#define	TPU_STAT_LIST	(TPU_MAGIC | 14) 
#define	TPU_STATS	(TPU_MAGIC | 12)
#define	TPU_GSTATS	(TPU_MAGIC | 22)
#define	TPU_CONFIG	(TPU_MAGIC |  9)
#define	TPU_SET_FLAG	(TPU_MAGIC | 10)
#define	TPU_GET_FLAG	(TPU_MAGIC | 11)
#define	TPU_SET_FAULT	(TPU_MAGIC | 15)
#define	TPU_GET_FAULT	(TPU_MAGIC | 16)
#define	TPU_GET_SOFT	(TPU_MAGIC | 17) 
#define	TPU_GET_TRACE	(TPU_MAGIC | 18) 
#define	TPU_GET_GTRACE	(TPU_MAGIC | 21) 
#define	TPU_EXT_TEST	(TPU_MAGIC | 20) 

/*
 * TPU_RUN ioctl argument structure.
 */
typedef struct tpud_run_s {
	__uint64_t		tpu_status;			/* driver error code */
	__uint64_t		dmaStatusReg;			/* Xtalk interrupt status register (0x100) */
	__uint64_t		ldiStatusReg;			/* LDI register 10: tStatusReg */
	__uint64_t		ldiTimerReg;			/* LDI register 19: tTimerReg */
	__uint64_t		_reserved_0[2];
	__uint64_t		dma0Status;			/* DMA 0 status */
	__uint64_t		dma0Count;			/* DMA 0 words output */
	__uint64_t		dma0Ticks;			/* DMA 0 output time (10ns ticks) */
	__uint64_t		dma1Status;			/* DMA 1 status */
	__uint64_t		dma1Count;			/* DMA 1 words input */
	__uint64_t		dma1Ticks;			/* DMA 1 input time (10ns ticks) */
	__uint64_t		sysStartClock;			/* RTC at start of TPU_RUN ioctl */
	__uint64_t		tpuStartClock;			/* RTC when TPU starts */
	__uint64_t		tpuEndClock;			/* RTC when TPU completes */
	__uint64_t		sysEndClock;			/* RTC at end of TPU_RUN ioctl */
	__uint64_t		device;				/* Device number that processed request */
	__uint64_t		_reserved_1[11];
	__uint64_t		timeout;			/* timeout in seconds */
	__uint64_t		ldi_offset;			/* byte offset to start of LDI coef */
	__uint64_t		page_size;			/* page size in bytes */
	__uint64_t		page_count;			/* number of active pages */
	__uint64_t		page_addr[TPU_PAGES_MAX];	/* page table */

} tpud_run_t;

/*
 * Only the first TPU_RUN_HDR_LEN bytes of the tpud_run_s structure will be
 * updated by the driver after the ioctl() completes.
 */
#define	TPU_RUN_HDR_LEN		((size_t) & (((tpud_run_t *) 0)->_reserved_1[0]))

/*
 * TPU_STATS ioctl argument structure.
 */
typedef	struct tpud_event_s {
	__uint64_t		timestamp;		/* time of most recent occurrence */
	__uint64_t		count;			/* number of occurrences */

} tpud_event_t;

typedef volatile struct tpud_stats_s {
	tpud_event_t		open;			/* open call */
	tpud_event_t		close;			/* close call */
	tpud_event_t		mmap;			/* mmap call */
	tpud_event_t		munmap;			/* munmap call */
	tpud_event_t		ioctlRun;		/* ioctl_run call */
	tpud_event_t		ioctlResume;		/* ioctl_resume call */
	tpud_event_t		ioctlHalt;		/* ioctl_halt call */
	tpud_event_t		ioctlRegs;		/* ioctl_regs call */
	tpud_event_t		ioctlInst;		/* ioctl_inst call */
	tpud_event_t		ioctlStat;		/* ioctl_stat call */
	tpud_event_t		ioctlStatList;		/* ioctl_stat_list call */
	tpud_event_t		ioctlStats;		/* ioctl_stats call */
	tpud_event_t		ioctlConfig;		/* ioctl_config call */
	tpud_event_t		ioctlSetFlag;		/* ioctl_set_flag call */
	tpud_event_t		ioctlGetFlag;		/* ioctl_get_flag call */
	tpud_event_t		ioctlSetFault;		/* ioctl_set_fault call */
	tpud_event_t		ioctlGetFault;		/* ioctl_get_fault call */
	tpud_event_t		ioctlGetSoft;		/* ioctl_get_soft call */
	tpud_event_t		ioctlGetTrace;		/* ioctl_get_trace call */
	tpud_event_t		ioctlGetGTrace;		/* ioctl_get_gtrace call */
	tpud_event_t		ioctlGstats;		/* ioctl_gstats call */
	tpud_event_t		ioctlExtTest;		/* ioctl_ext_test call */
	tpud_event_t		_reserved1[26];
	tpud_event_t		immediateFault;		/* immediate fault generated */
	tpud_event_t		deferredFault;		/* deferred fault generated */
	tpud_event_t		timeoutFault;		/* timeout fault generated */
	tpud_event_t		page[6];		/* Count of each page size loaded */
	tpud_event_t		timeout;		/* timeouts */
	tpud_event_t		dmaError;		/* widget error interrupts */
	tpud_event_t		ldiBarrier;		/* LDI barrier interrupts */
	tpud_event_t		ldiError;		/* LDI error interrupts */
	tpud_event_t		ldiCError;		/* LDI channel error interrupts */
	__uint64_t		dma0Count;		/* Words output (host -> TPU) */
	__uint64_t		dma0Ticks;		/* Output time (10ns ticks) */
	__uint64_t		dma1Count;		/* Words input (TPU -> host) */
	__uint64_t		dma1Ticks;		/* Input time (10ns ticks) */

} tpud_stats_t;

/*
 * TPU_GSTATS ioctl argument structure.
 */
typedef volatile struct tpud_gstats_s {
	tpud_event_t		assign_spec;		/* assignments to specific devices */
	tpud_event_t		assign_any;		/* assignments to "any" device */
	tpud_event_t		assign_chk;		/* devices checked */
	tpud_event_t		assign_none;		/* no devices available */

} tpud_gstats_t;

/*
 * TPU_STAT ioctl argument structure.
 */
typedef struct tpud_stat_s {
	char			name[TPU_NAME_MAX];	/* device name */
	__uint32_t		vhdl;			/* tpu vhdl */
	__uint32_t		admin;			/* admin vhdl */
	__uint32_t		online;			/* online flag */
	__uint32_t		state;			/* device state */
	__uint32_t		open;			/* open count */
	__uint32_t		exclusive;		/* exclusive open flag */
	__uint32_t		simulated;		/* simulated device */
	__uint32_t		module;			/* physical module number */
	__uint32_t		slot;			/* physical io slot number */
	__uint32_t		_reserved[5];
	__uint64_t		flag;			/* user flag */
	__uint64_t		dma0Count;		/* Words output (host -> TPU) */
	__uint64_t		dma0Ticks;		/* Output time (10ns ticks) */
	__uint64_t		dma1Count;		/* Words input (TPU -> host) */
	__uint64_t		dma1Ticks;		/* Input time (10ns ticks) */

} tpud_stat_t;

/*
 * TPU_STAT_LIST ioctl argument structure.
 */
typedef struct tpud_stat_list_s {
	__uint32_t		max;			/* max number of stat structures to return*/
	__uint32_t		count;			/* number of stat structures actually returned */
	tpud_stat_t		stat[1];		/* array of stat blocks */

} tpud_stat_list_t;

#define	TPU_STAT_LIST_HDR_LEN	((size_t) & (((tpud_stat_list_t *) 0)->stat[0]))

/*
 * TPU_REGS ioctl argument structure.
 */
typedef struct tpud_regs_xtk_regs_s {

	__uint32_t		id;			/* 0x0_0004 Identification register */
	__uint32_t		status;			/* 0x0_000c Crosstalk Status register */
	__uint32_t		err_upper_addr;		/* 0x0_0014 Error Upper Address reg */
	__uint32_t		err_lower_addr;		/* 0x0_001c Error Address register */
	__uint32_t		control;		/* 0x0_0024 Crosstalk Control reg */
	__uint32_t		req_timeout;		/* 0x0_002c Request Time-out Value */
	__uint32_t		intdest_upper_addr;	/* 0x0_0034 Intrrupt Destin Addr Upper */
	__uint32_t		intdest_lower_addr;	/* 0x0_003c Interrupt Destination Addr */
	__uint32_t		err_cmd_word;		/* 0x0_0044 Error Command Word reg */
	__uint32_t		llp_cfg;		/* 0x0_004c LLP Configuration reg */
	__uint32_t		ede;			/* 0x0_0064 Error Data Enable reg */
	__uint32_t		int_status;		/* 0x0_0104 Interrupt Status register */
	__uint32_t		int_enable;		/* 0x0_010c Interrupt Enable register */
	__uint32_t		int_addr0;		/* 0x0_0134 Aux Interrupt 0 Addr reg */
	__uint32_t		int_addr1;		/* 0x0_013c Aux Interrupt 1 Addr reg */
	__uint32_t		int_addr2;		/* 0x0_0144 Aux Interrupt 2 Addr reg */
	__uint32_t		int_addr3;		/* 0x0_014c Aux Interrupt 3 Addr reg */
	__uint32_t		int_addr4;		/* 0x0_0154 Aux Interrupt 4 Addr reg */
	__uint32_t		int_addr5;		/* 0x0_015c Aux Interrupt 5 Addr reg */
	__uint32_t		int_addr6;		/* 0x0_0164 Aux Interrupt 6 Addr reg */
	__uint32_t		sense;			/* 0x0_0184 User Definable Sense reg */
	__uint32_t		leds;			/* 0x0_018c User Definable Enable reg */

} tpud_regs_xtk_regs_t;

#define TPU_ATT_SIZE		128

typedef struct tpud_regs_att_regs_s {
	__uint32_t		config;			/* 0x8_0004 Address Translation Configuration reg */
	__uint32_t		diag;			/* 0x8_000c Address Translation Diagnostic reg */
	__uint64_t		atte[TPU_ATT_SIZE];	/* 0x8_0400 Address Translation Table */

} tpud_regs_att_regs_t;

typedef struct tpud_regs_dma_regs_s {
	__uint64_t		config0;		/* 0x10_0000 Configuration Reg 0 */
	__uint64_t		config1;		/* 0x10_0008 Configuration Reg 1 */
	__uint64_t		status;			/* 0x10_0010 Status register */
	__uint64_t		diag_mode;		/* 0x10_0018 Diagnostic Mode register */
	__uint64_t		perf_timer;		/* 0x10_0020 Performance Timer reg */
	__uint64_t		perf_count;		/* 0x10_0028 Performance Counter reg */
	__uint64_t		perf_config;		/* 0x10_0030 Performance Config reg */

} tpud_regs_dma_regs_t;

typedef struct tpud_regs_ldi_regs_s {
	__uint64_t		riscIOpcode;		/*  0 0x30_02e0 Risc Loaded Op Instruction */
	__uint64_t		riscMode;		/*  1 0x30_02e8 using Risc to operate LSP */
	__uint64_t		xRiscSadr;		/*  2 0x30_02f0 Risc loaded starting addr */
	__uint64_t		coefBs;			/*  3 0x30_02f8 Coef Block Size */
	__uint64_t		coefInc;		/*  4 0x30_0300 Coef Incrementer */
	__uint64_t		coefInit;		/*  5 0x30_0308 Coef Initial data */
	__uint64_t		dataBs;			/*  6 0x30_0310 Data Block Size */
	__uint64_t		saBlkInc;		/*  7 0x30_0318 Start Addr Blk Incrementer */
	__uint64_t		saBlkSize;		/*  8 0x30_0320 Start Addr Blk Size */
	__uint64_t		saBlkInit;		/*  9 0x30_0328 Start Addr Blk Initial */
	__uint64_t		statusReg;		/* 10 0x30_0330 Status readback register */
	__uint64_t		sampInit;		/* 11 0x30_0338 Receive sample data initial */
	__uint64_t		vectInitInc;		/* 12 0x30_0340 Rec SA Vector init Incr */
	__uint64_t		vectInit;		/* 13 0x30_0348 Rec SA Vector initial */
	__uint64_t		vectInc;		/* 14 0x30_0350 Rec SA Vector incrementer */
	__uint64_t		errorMask;		/* 15 0x30_0358 Error interrupt mask */
	__uint64_t		lbaBlkSize;		/* 16 0x30_0360 LBA block size */
	__uint64_t		loopCnt;		/* 17 0x30_0368 Loop counter for i-que */
	__uint64_t		diagReg;		/* 18 0x30_0370 Diagnostic Readback register */
	__uint64_t		iSource;		/* 19 0x30_0378 Timer/barrier source reg */
	__uint64_t		pipeInBus[8];		/* 20 0x30_0380 -> 27 0x30_03b8 xmit Load pipe In bus size */
	__uint64_t		rRiscSadr;		/* 28 0x30_03c0 */
	__uint64_t		sampInc;		/* 29 0x30_03c8 Receive sample increment */
	__uint64_t		oLineSize;		/* 30 0x30_03d0 Receive output line size */
	__uint64_t		iMask;			/* 31 0x30_03d8 */
	__uint64_t		tFlag;			/* 32 0x30_03e0 */
	__uint64_t		bFlag;			/* 33 0x30_03e8 */
	__uint64_t		diagPart;		/* 34 0x30_03f0 */
	__uint64_t		jmpCnd;			/* 35 0x30_03f8 */

} tpud_regs_ldi_regs_t;

typedef struct tpud_regs_s {
	tpud_regs_xtk_regs_t	xtk_regs;
	tpud_regs_att_regs_t	att_regs;
	tpud_regs_dma_regs_t	dma0_regs;
	tpud_regs_dma_regs_t	dma1_regs;
	tpud_regs_ldi_regs_t	ldi_regs;

} tpud_regs_t;


/*
 * TPU_INST ioctl argument structure.
 */
#define TPU_LDI_I_SIZE		64
#define	TPU_LDI_I_UPPER		0
#define	TPU_LDI_I_LOWER		1

typedef struct tpud_inst_s {
	__uint64_t		inst[TPU_LDI_I_SIZE][2];/*    0x30_0400 Instruction memory*/

} tpud_inst_t;

/*
 * TPU_CONFIG argument values.
 */
#define TPU_CONFIG_DOWN		0
#define TPU_CONFIG_UP		1

/*
 * TPU_SET_FAULT/TPU_GET_FAULT ioctl argument structure.
 */
typedef enum {

	TPU_FAULT_NONE = 0,
	TPU_FAULT_IMMEDIATE,
	TPU_FAULT_DEFERRED,
	TPU_FAULT_TIMEOUT

} tpud_fault_type_t;

typedef struct tpud_fault_s {
	tpud_fault_type_t	type;			/* fault type */
 	__int64_t		cycles;			/* total number of fault cycles */
	__int64_t		interval;		/* interval between faults */
	__int64_t		duration;		/* number of times to repeat fault */
	__uint64_t		tpu_status;		/* driver error code */
	__uint64_t		_reserved_0[9];
	__uint64_t		dmaStatusReg;		/* DMA interrupt status register */
	__uint64_t		dmaStatusReg_mask;	/* mask for DMA interrupt status register */
	__uint64_t		ldiStatusReg;		/* LDI register 10: tStatusReg */
	__uint64_t		ldiStatusReg_mask;	/* mask for LDI register 10: tStatusReg */
	__uint64_t		ldiTimerReg;		/* LDI register 19: tTimerReg */
	__uint64_t		ldiTimerReg_mask;	/* mask for LDI register 19: tTimerReg */
	__uint64_t		dma0Status;		/* DMA 0 status */
	__uint64_t		dma0Status_mask;	/* mask for DMA 0 status */
	__uint64_t		dma1Status;		/* DMA 1 status */
	__uint64_t		dma1Status_mask;	/* mask for DMA 1 status */

} tpud_fault_t;

/*
 * TPU_GET_TRACE structures.
 */
#define TPU_TRACEB_LEN		1024			/* entries in a trace buffer (must be 2**N) */
#define TPU_TRACEB_MAXP		5			/* max number of parameters in a trace entry */
#define	TPU_TRACEB_MAXF		256			/* max length of a format string */

/*
 *	Trace entry.
 */
typedef struct tpud_tracee_s {

	__int64_t		timestamp;		/* timestamp */
	__int32_t		cpu;			/* cpu */
	__int32_t		_reserved;
	char *			fmt;			/* format string pointer */
	__uint64_t		param[TPU_TRACEB_MAXP];	/* parameters */

} tpud_tracee_t;

/*
 *	Trace buffer.
 */
typedef struct tpud_traceb_s {

	__uint32_t		magic;			/* magic number (TPU_M_TRACE) */
	__int32_t		_reserved_0;
	__int64_t		count;			/* total number of trace entries made */
	__int64_t		create_sec;		/* system seconds at buffer creation */
	__int64_t		create_rtc;		/* RTC at buffer creation */
	__int32_t		rtc_nsec;		/* RTC clock period in nsec */
	__int32_t		_reserved_1[7];
	tpud_tracee_t		e[TPU_TRACEB_LEN];	/* trace entries */

} tpud_traceb_t;

/*
 *	Exportable copy of trace buffer.
 */
typedef struct tpud_tracex_s {

	tpud_traceb_t		t;					/* trace buffer */
	char			f[TPU_TRACEB_LEN][TPU_TRACEB_MAXF];	/* format strings */

} tpud_tracex_t;

#ifdef __cplusplus
}
#endif

#endif /* __SYS_TPUCOM_H__ */
