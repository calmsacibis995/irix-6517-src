/**************************************************************************
 *                                                                        *
 * 		 Copyright (C) 1993 Silicon Graphics, Inc.                        *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

/*
 * apftest.h - Adobe Pixel Formatter ROM diagnostics and IDE
 */

typedef struct {
	unsigned int cmd_stat;     /* test_id, err_code, param_cnt, heartbeat */
	unsigned int p_misc;
	unsigned int p_exp;           /* expected result */
	unsigned int p_act;           /* actual result */
	unsigned int p_other;
} pf_diag_status_blk;

/* 
 * defines for various sub-fields of the cmd_stat field.
 */

#define TEST_ID       0x000000ff
#define ERROR_CODE    0x0000ff00
#define PARAM_CNT     0x00ff0000
#define HEART_BEAT    0x80000000

#define PASSED        0x00000100
#define PASSED_PON	  0x40000000
#define IDLE          0			/* test id == 0 68302 idle */
#define RUNNING       0

/* return code */
#define FAILED	1

/* special offsets into pixel formatter board sram */

#define PF_DIAG_STATUS_OFFSET       0x1ff00     /* 68302 status/comm blk */

/* test pattern */
#define END_OF_PAT	0xdeadface
