/* l64861.h - (SEC) LSI Logic L64861 Sbus to Ebus Interface header. */

/* Copyright 1994-1995 Wind River Systems, Inc. */

/* Copyright 1994-1995 FORCE COMPUTERS */

/*
modification history
--------------------
01b,15mar95,vin  integrated with 5.2.
01a,14jun93,tkj  written
*/

/* _OFS = Offset from chip registers, i.e. L64862_SEC_PFN in l64862.h */
/* This can be used to compute both the virtual and physical addresses. */

#ifndef __INCl64861h
#define __INCl64861h

#ifdef __cplusplus
extern "C" {
#endif

/* Keyboard/Mouse Registers */
#define L64861_KEYBOARD_REGS_OFS 0x00000000 /* Keyboard/Mouse Registers */
#define L64861_KEYBOARD_KECM_OFS 0x00000000 /* Control channel Mouse */
#define L64861_KEYBOARD_KEDM_OFS 0x00000002 /* Data channel Mouse */
#define L64861_KEYBOARD_KECK_OFS 0x00000004 /* Control channel Keyboard */
#define L64861_KEYBOARD_KEDK_OFS 0x00000006 /* Data channel Keyboard */

/* Serial Port Registers */
#define L64861_SERIAL_REGS_OFS	0x00100000 /* Serial Port Registers */
#define L64861_SERIAL_SCCB_OFS	0x00100000 /* Control channel B */
#define L64861_SERIAL_SCDB_OFS	0x00100002 /* Data channel B */
#define L64861_SERIAL_SCCA_OFS	0x00100004 /* Control channel A */
#define L64861_SERIAL_SCDA_OFS	0x00100006 /* Data channel A */

/* Time-of-day clock and Nonvolatile RAM */
#define	L64861_NVRAM_OFS	0x00200000 /* Nonvolatile RAM */

/* Counter/Timer Registers */
#define L64861_TIMER_REGS_OFS	   0x00300000

/* Processor 0 Counter/Timer Registers */
/* Counter-Timer mode */
#define L64861_CT_LIMIT_PROC0_OFS	0x00300000 /* Counter limit */
#define	L64861_CT_COUNTER_PROC0_OFS	0x00300004 /* Counter */
#define L64861_CT_LIMIT_NOR_PROC0_OFS	0x00300008 /* Counter limit: No reset*/
/* User Timer mode */
#define L64861_UT_TIMER_MSW_PROC0_OFS	0x00300000 /* User timer MSW */
#define	L64861_UT_TIMER_LSW_PROC0_OFS	0x00300004 /* User timer LSW */
#define L64861_UT_START_PROC0_OFS	0x0030000c /* User timer start stop */

/* Processor 1 Counter/Timer Registers */
/* Counter-Timer mode */
#define L64861_CT_LIMIT_PROC1_OFS	0x00301000 /* Counter limit */
#define	L64861_CT_COUNTER_PROC1_OFS	0x00301004 /* Counter */
#define L64861_CT_LIMIT_NOR_PROC1_OFS	0x00301008 /* Counter limit: No reset*/
/* User Timer mode */
#define L64861_UT_TIMER_MSW_PROC1_OFS	0x00301000 /* User timer MSW */
#define	L64861_UT_TIMER_LSW_PROC1_OFS	0x00301004 /* User timer LSW */
#define L64861_UT_START_PROC1_OFS	0x0030100c /* User timer start stop */

/* Processor 2 Counter/Timer Registers */
/* Counter-Timer mode */
#define L64861_CT_LIMIT_PROC2_OFS	0x00302000 /* Counter limit */
#define	L64861_CT_COUNTER_PROC2_OFS	0x00302004 /* Counter */
#define L64861_CT_LIMIT_NOR_PROC2_OFS	0x00302008 /* Counter limit: No reset*/
/* User Timer mode */
#define L64861_UT_TIMER_MSW_PROC2_OFS	0x00302000 /* User timer MSW */
#define	L64861_UT_TIMER_LSW_PROC2_OFS	0x00302004 /* User timer LSW */
#define L64861_UT_START_PROC2_OFS	0x0030200c /* User timer start stop */

/* Processor 3 Counter/Timer Registers */
/* Counter-Timer mode */
#define L64861_CT_LIMIT_PROC3_OFS	0x00303000 /* Counter limit */
#define	L64861_CT_COUNTER_PROC3_OFS	0x00303004 /* Counter */
#define L64861_CT_LIMIT_NOR_PROC3_OFS	0x00303008 /* Counter limit: No reset*/
/* User Timer mode */
#define L64861_UT_TIMER_MSW_PROC3_OFS	0x00303000 /* User timer MSW */
#define	L64861_UT_TIMER_LSW_PROC3_OFS	0x00303004 /* User timer LSW */
#define L64861_UT_START_PROC3_OFS	0x0030300c /* User timer start stop */

/* System Counter/Timer Registers */
/* Counter-Timer mode */
#define L64861_CT_LIMIT_SYS_OFS		0x00310000 /* Counter limit */
#define	L64861_CT_COUNTER_SYS_OFS	0x00310004 /* Counter */
#define L64861_CT_LIMIT_NOR_SYS_OFS	0x00310008 /* Counter limit: No reset*/
#define L64861_CONFIG_SYS_OFS		0x00310010 /* Timer configuration */

/*
 * System Timer Configuration Register
 */

#define L64861_CONFIG_SYS_T0_MASK  0x00000001 /* User/System Mode select */
#define L64861_CONFIG_SYS_T0_USER  0x00000001 /* User Counter Mode (54 bits) */
#define L64861_CONFIG_SYS_T0_SYS   0x00000000 /* System Timer Mode (22 bits) */

#define L64861_CONFIG_SYS_T1_MASK  0x00000002 /* User/System Mode select */
#define L64861_CONFIG_SYS_T1_USER  0x00000002 /* User Counter Mode (54 bits) */
#define L64861_CONFIG_SYS_T1_SYS   0x00000000 /* System Timer Mode (22 bits) */

#define L64861_CONFIG_SYS_T2_MASK  0x00000004 /* User/System Mode select */
#define L64861_CONFIG_SYS_T2_USER  0x00000004 /* User Counter Mode (54 bits) */
#define L64861_CONFIG_SYS_T2_SYS   0x00000000 /* System Timer Mode (22 bits) */

#define L64861_CONFIG_SYS_T3_MASK  0x00000008 /* User/System Mode select */
#define L64861_CONFIG_SYS_T3_USER  0x00000008 /* User Counter Mode (54 bits) */
#define L64861_CONFIG_SYS_T3_SYS   0x00000000 /* System Timer Mode (22 bits) */

/* Interrupt Registers */
#define	L64861_PIR_REGS_OFS	   0x00400000

/* Processor 0 Interrupt Registers */
#define	L64861_PIR0_PENDING_OFS   0x00400000 /* Interrupt Pending */
#define	L64861_PIR0_CLR_OFS       0x00400004 /* Int. Pending Clear */
#define	L64861_PIR0_SET_OFS       0x00400008 /* Int. Pending Set */

/* Processor 1 Interrupt Registers */
#define	L64861_PIR1_PENDING_OFS   0x00401000 /* Interrupt Pending */
#define	L64861_PIR1_CLR_OFS       0x00401004 /* Int. Pending Clear */
#define	L64861_PIR1_SET_OFS       0x00401008 /* Int. Pending Set */

/* Processor 2 Interrupt Registers */
#define	L64861_PIR2_PENDING_OFS   0x00402000 /* Interrupt Pending */
#define	L64861_PIR2_CLR_OFS       0x00402004 /* Int. Pending Clear */
#define	L64861_PIR2_SET_OFS       0x00402008 /* Int. Pending Set */

/* Processor 3 Interrupt Registers */
#define	L64861_PIR3_PENDING_OFS   0x00403000 /* Interrupt Pending */
#define	L64861_PIR3_CLR_OFS       0x00403004 /* Int. Pending Clear */
#define	L64861_PIR3_SET_OFS       0x00403008 /* Int. Pending Set */

/* System Interrupt Registers */
#define	L64861_IR_PENDING_OFS     0x00410000 /* Interrupt Pending */
#define L64861_IR_MASK_OFS	  0x00410004 /* Int. Mask */
#define	L64861_IR_MASK_CLR_OFS    0x00410008 /* Int. Mask Clear */
#define	L64861_IR_MASK_SET_OFS	  0x0041000c /* Int. Mask Set */
#define L64861_IR_TARGET_OFS      0x00410010 /* Int. Target */

/*
 * Bit masks for system interrupt registers:
 * L64861_IR_PENDING, L64861_IR_MASK, L64861_IR_MASK_CLR, L64861_IR_MASK_SET
 */

#define	IR_RESERVED	0x0f80007f	/* All reserved bits */

#define IR_ALL			(1 << 31) /* All interupts */
#define	IR_ANYMODERR		(1 << 30) /* 15 Module Error: Any module */
#define	IR_MSWRBUFERR		(1 << 29) /* 15 M to S Bus Write Buffer Err */
#define	IR_ECCMEMERR		(1 << 28) /* 15 ECC Memory Error */
#define	IR_FLOPPY		(1 << 22) /* 11 Floppy */
#define	IR_VMEMODERR		(1 << 21) /*  9 Module Error: Not IU */
#define	IR_VIDEO		(1 << 20) /*  8 On-board Video */
#define IR_CLOCK10		(1 << 19) /* 10 System Clock */
#define	IR_SCSI			(1 << 18) /*  4 SCSI */
#define	IR_AUDIO		(1 << 17) /* 13 Audio/ISDN */
#define	IR_ETHERNET		(1 << 16) /*  6 Ethernet */
#define	IR_SERIAL		(1 << 15) /* 12 Serial */
#define	IR_KEYBOARD		(1 << 14) /* 12 Keyboard */
#define	IR_SBUS7		(1 << 13) /* 13 SBusIrq 7 */
#define	IR_SBUS6		(1 << 12) /* 11 SBusIrq 6 */
#define	IR_SBUS5		(1 << 11) /*  9 SBusIrq 5 */
#define	IR_SBUS4		(1 << 10) /*  7 SBusIrq 4 */
#define	IR_SBUS3		(1 <<  9) /*  5 SBusIrq 3 */
#define	IR_SBUS2		(1 <<  8) /*  3 SBusIrq 2 */
#define	IR_SBUS1		(1 <<  7) /*  2 SBusIrq 1 */

/*
 * Bit masks for processor interrupt registers:
 * L64861_PIRx_PENDING, L64861_PIRx_SET, L64861_PIRx_CLR
 */

/* L64861_PIRx_PENDING, L64861_PIRx_SET, or L64861_PIRx_CLR */
#define	PIR_SOFT15		(1 << 31) /* 15 Software Interrupt 15 */
#define	PIR_SOFT14	        (1 << 30) /* 14 Software Interrupt 14 */
#define	PIR_SOFT13              (1 << 29) /* 13 Software Interrupt 13 */
#define	PIR_SOFT12		(1 << 28) /* 12 Software Interrupt 12 */
#define	PIR_SOFT11		(1 << 27) /* 11 Software Interrupt 11 */
#define	PIR_SOFT10		(1 << 26) /* 10 Software Interrupt 10 */
#define	PIR_SOFT9               (1 << 25) /*  9 Software Interrupt  9 */
#define	PIR_SOFT8               (1 << 24) /*  8 Software Interrupt  8 */
#define	PIR_SOFT7               (1 << 23) /*  7 Software Interrupt  7 */
#define	PIR_SOFT6               (1 << 22) /*  6 Software Interrupt  6 */
#define	PIR_SOFT5               (1 << 21) /*  5 Software Interrupt  5 */
#define	PIR_SOFT4               (1 << 20) /*  4 Software Interrupt  4 */
#define	PIR_SOFT3               (1 << 19) /*  3 Software Interrupt  3 */
#define	PIR_SOFT2               (1 << 18) /*  2 Software Interrupt  2 */
#define	PIR_SOFT1               (1 << 17) /*  1 Software Interrupt  1 */

/* L64861_PIRx_PENDING, or L64861_PIRx_CLR */
#define	PIR_HARD15		(1 << 15) /* 15 Hardware Interrupt 15 */
#define	PIR_ASYNCH	PIR_HARD15	/* 15 Asynchronous Error (broadcast) */

/* L64861_PIRx_PENDING */
#define	PIR_RESERVED	0x00010001	/* All reserved bits */

#define	PIR_HARD14	        (1 << 14) /* 14 Hardware Interrupt 14 */
#define	PIR_CLOCK14	PIR_HARD14	/* 14 Pre Processor Counter/Timer */

#define	PIR_HARD13              (1 << 13) /* 13 Hardware Interrupt 13 */
#define	PIR_SBUS7	PIR_HARD13	/* 13 SBusIrq 7 */
#define	PIR_AUDIO	PIR_HARD13	/* 13 Audio/ISDN */

#define	PIR_HARD12		(1 << 12) /* 12 Hardware Interrupt 12 */
#define	PIR_KEYBOARD	PIR_HARD12	/* 12 Keyboard */
#define	PIR_SERIAL	PIR_HARD12	/* 12 Serial */

#define	PIR_HARD11		(1 << 11) /* 11 Hardware Interrupt 11 */
#define	PIR_SBUS6	PIR_HARD11	/* 11 SBusIrq 6 */
#define	PIR_FLOPPY	PIR_HARD11	/* 11 Floppy */

#define	PIR_HARD10		(1 << 10) /* 10 Hardware Interrupt 10 */
#define	PIR_CLOCK10	PIR_HARD10	/* 10 System Clock */

#define	PIR_HARD9               (1 <<  9) /*  9 Hardware Interrupt  9 */
#define	PIR_SBUS5	PIR_HARD9	/* 9 SBusIrq 5 */
#define	PIR_VMEMODERR	PIR_HARD9	/* 9 Module Error: Not IU */

#define	PIR_HARD8               (1 <<  8) /*  8 Hardware Interrupt  8 */
#define	PIR_VIDEO	PIR_HARD8	/* 8 On-board Video */

#define	PIR_HARD7               (1 <<  7) /*  7 Hardware Interrupt  7 */
#define	PIR_SBUS4	PIR_HARD7	/* 7 SBusIrq 4 */

#define	PIR_HARD6               (1 <<  6) /*  6 Hardware Interrupt  6 */
#define	PIR_ETHERNET	PIR_HARD6	/* 6 Ethernet */

#define	PIR_HARD5               (1 <<  5) /*  5 Hardware Interrupt  5 */
#define	PIR_SBUS3	PIR_HARD5	/* 5 SBusIrq 3 */

#define	PIR_HARD4               (1 <<  4) /*  4 Hardware Interrupt  4 */
#define	PIR_SCSI	PIR_HARD4	/* 4 SCSI */

#define	PIR_HARD3               (1 <<  3) /*  3 Hardware Interrupt  3 */
#define	PIR_SBUS2	PIR_HARD3	/* 3 SBusIrq 2 */

#define	PIR_HARD2               (1 <<  2) /*  2 Hardware Interrupt  2 */
#define	PIR_SBUS1	PIR_HARD2	/* 2 SBusIrq 1 */

/* Audio Port */
#define L64861_AUDIO_OFS	       0x00500000 /* Audio Port */

#define	L64861_AUD_IC_OFS	       0x00500000 /* Interrupt Command */
#define L64861_AUD_DR_OFS	       0x00500001 /* Data Register */
#define	L64861_AUD_DS_OFS	       0x00500002 /* D-Channel Status */
#define	L64861_AUD_DE_OFS	       0x00500003 /* D-Channel Error */
#define	L64861_AUD_DRX_OFS	       0x00500004 /* D-Channel Rcve-Xmit */
#define	L64861_AUD_BbRX_OFS	       0x00500005 /* Bb-Channel Rcve-Xmit */
#define	L64861_AUD_BcRX_OFS	       0x00500006 /* Bc-Channel Rcve-Xmit */
#define	L64861_AUD_DS2_OFS	       0x00500007 /* D-Channel Status 2 */

/* Floppy Port */
#define L64861_FLOPPY_OFS	       0x00700000 /* Floppy Port */
#define L64861_FLOP_MS_OFS	       0x00700000 /* Main Status (Read) */
#define L64861_FLOP_DRS_OFS	       0x00700000 /* Data Rate Select (Write)*/
#define	L64861_FLOP_FDP_OFS	       0x00700001 /* FIFO Data Port */

/* Aux1 Port */
#define	L64861_AUX1_OFS			0x00800000 /* Aux1 port */

#define	L64861_AUX1_L_MASK		0x01 /* LED (Write only) */
#define	L64861_AUX1_L_ON		0x01
#define	L64861_AUX1_L_OFF		0

#define	L64861_AUX1_E_MASK		0x02 /* Floppy eject (Write only) */
#define	L64861_AUX1_E_SET		0x02
#define	L64861_AUX1_E_CLEARED		0

			/* Floppy controller terminal count (Write only) */
#define	L64861_AUX1_T_MASK		0x04
#define	L64861_AUX1_T_SET		0x04
#define	L64861_AUX1_T_CLEARED		0

					/* Floppy drive select (Write only) */
#define	L64861_AUX1_S_MASK		0x08
#define	L64861_AUX1_S_SELECT		0x08
#define	L64861_AUX1_S_SELECT_NOT	0

					/* Floppy diskette change (Read only)*/
#define	L64861_AUX1_C_MASK		0x10
#define	L64861_AUX1_C_PRESENT		0
#define	L64861_AUX1_C_PRESENT_NOT	0x10

					/* Floppy density (Read only) */
#define	L64861_AUX1_D_MASK		0x20
#define	L64861_AUX1_D_HIGH		0x20
#define	L64861_AUX1_D_LOW		0

/* Generic Port */
#define	L64861_GENERIC_OFS		0x00a00000 /* Generic port */

/* Aux2 Port */
#define	L64861_AUX2_OFS			0x00a01000 /* Aux2 port */
#define	L64861_AUX2_OUT0_MASK		0x01 /* Write only */
#define	L64861_AUX2_OUT1_MASK		0x02 /* Write only */
#define	L64861_AUX2_IN0_MASK		0x10 /* Read only */
#define	L64861_AUX2_IN1_MASK		0x20 /* Read only */

/* System Status Register */
#define	L64861_SYS_STATUS_OFS	0x00f00000 /* System Status Register */

#ifndef	_ASMLANGUAGE
#endif	/* _ASMLANGUAGE */

#ifdef __cplusplus
}
#endif

#endif /* __INCl64861h */
