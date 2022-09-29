/*
**               Copyright (C) 1989, Silicon Graphics, Inc.
**
**  These coded instructions, statements, and computer programs  contain
**  unpublished  proprietary  information of Silicon Graphics, Inc., and
**  are protected by Federal copyright law.  They  may  not be disclosed
**  to  third  parties  or copied or duplicated in any form, in whole or
**  in part, without the prior written consent of Silicon Graphics, Inc.
*/

/*
**      FileName:       mgv_csc_util.c
*/

#include "mgv_diag.h"

#define ABDIR_REG	0
#define CCDIR_REG	1
#define CCIND_REG	2
#define GALDIR_REG	3
#define GALIND_REG	4
#define TMI_REG		5
#define CSC_REG		6

/* forward defines */
__uint32_t _mgv_register_rdwr(__uint32_t, __uint32_t, __uint32_t);


/*
 * mgv_galind_wr  -- writes galind registers
 *
 * -r <register number>
 * -d <data>
 *
 */
__uint32_t mgv_galind_wr(__int32_t argc, char **argv)
{
	__uint32_t reg, val;

	GET_OPT;
	_mgv_register_rdwr(GALIND_REG, reg, val, 0);
}

/*
 * mgv_galind_rd  -- reads galind registers
 *
 * -r <register number>
 *
 */
__uint32_t mgv_galind_rd(__int32_t argc, char **argv)
{
	__uint32_t reg, val;

	GET_OPT;
	_mgv_register_rdwr(GALIND_REG, reg, val, 1);
}

/*
 * mgv_galdir_wr  -- writes galdir registers
 *
 * -r <register number>
 * -d <data>
 *
 */
__uint32_t mgv_galdir_wr(__int32_t argc, char **argv)
{
	__uint32_t reg, val;

	GET_OPT;
	_mgv_register_rdwr(GALDIR_REG, reg, val, 0);
}

/*
 * mgv_galdir_rd  -- reads galdir registers
 *
 * -r <register number>
 *
 */
__uint32_t mgv_galdir_rd(__int32_t argc, char **argv)
{
	__uint32_t reg, val;

	GET_OPT;
	_mgv_register_rdwr(GALDIR_REG, reg, val, 1);
}

/*
 * mgv_ccind_wr  -- writes ccind registers
 *
 * -r <register number>
 * -d <data>
 *
 */
__uint32_t mgv_ccind_wr(__int32_t argc, char **argv)
{
	__uint32_t reg, val;

	GET_OPT;
	_mgv_register_rdwr(CCIND_REG, reg, val, 0);
}

/*
 * mgv_ccind_rd  -- reads ccind registers
 *
 * -r <register number>
 *
 */
__uint32_t mgv_ccind_wr(__int32_t argc, char **argv)
{
	__uint32_t reg, val;

	GET_OPT;
	_mgv_register_rdwr(CCIND_REG, reg, val, 1);
}

/*
 * mgv_ccdir_wr  -- writes ccdir registers
 *
 * -r <register number>
 * -d <data>
 *
 */
__uint32_t mgv_ccdir_wr(__int32_t argc, char **argv)
{
	__uint32_t reg, val;

	GET_OPT;
	_mgv_register_rdwr(CCDIR_REG, reg, val, 0);
}

/*
 * mgv_ccdir_rd  -- reads ccdir registers
 *
 * -r <register number>
 *
 */
__uint32_t mgv_ccdir_rd(__int32_t argc, char **argv)
{
	__uint32_t reg, val;

	GET_OPT;
	_mgv_register_rdwr(CCDIR_REG, reg, val, 1);
}

/*
 * mgv_abind_wr  -- writes abind registers
 *
 * -r <register number>
 * -d <data>
 *
 */
__uint32_t mgv_abind_wr(__int32_t argc, char **argv)
{
	__uint32_t reg, val;

	GET_OPT;
	_mgv_register_rdwr(ABIND_REG, reg, val, 0);
}

/*
 * mgv_tmi_wr  -- writes tmi registers
 *
 * -r <register number>
 * -d <data>
 *
 */
__uint32_t mgv_tmi_wr(__int32_t argc, char **argv)
{
	__uint32_t reg, val;

	GET_OPT;
	_mgv_register_rdwr(TMI_REG, reg, val, 0);
}

/*
 * mgv_tmi_rd  -- reads tmi registers
 *
 * -r <register number>
 *
 */
__uint32_t mgv_tmi_rd(__int32_t argc, char **argv)
{
	__uint32_t reg, val;

	GET_OPT;
	_mgv_register_rdwr(TMI_REG, reg, val, 1);
}

/*
 * mgv_csc_wr  -- writes csc registers
 *
 * -r <register number>
 * -d <data>
 *
 */
__uint32_t mgv_csc_wr(__int32_t argc, char **argv)
{
	__uint32_t reg, val;

	GET_OPT;
	_mgv_register_rdwr(CSC_REG, reg, val, 0);
}

/*
 * mgv_csc_rd  -- reads csc registers
 *
 * -r <register number>
 *
 */
__uint32_t mgv_csc_rd(__int32_t argc, char **argv)
{
	__uint32_t reg, val;

	GET_OPT;
	_mgv_register_rdwr(CSC_REG, reg, val, 1);
}

/*
 * _mgv_register_rdwr  - general purpose routine to read/write any mgv reg
 *
 */
__uint32_t _mgv_register_rdwr(
	__uint32_t reg_type, 
	__uint32_t reg,
	__uint32_t val, 
	__uint32_t read_reg)
{
	switch (reg_type) {
	   case ABDIR_REG:
		if (read_reg) 
			AB_R1(mgvbase, reg, val)
		else
			AB_W1(mgvbase, reg, val);
		break;
	   case CCIND_REG:	
		if (read_reg) 
			CSC_IND_R1(mgvbase, reg, val)
		else
			CSC_IND_W1(mgvbase, reg, val);
		break;
	   case CCDIR_REG:	
		if (read_reg) 
			CC_R1(mgvbase, reg, val)
		else
			CC_W1(mgvbase, reg, val);
		break;
	   case GALIND_REG:	
		if (read_reg) 
			GAL_IND_R1(mgvbase, reg, val)
		else
			GAL_IND_W1(mgvbase, reg, val);
		break;
	   case GALDIR_REG:	
		if (read_reg) 
			GAL_R1(mgvbase, reg, val)
		else
			GAL_W1(mgvbase, reg, val);
		break;
	   case TMI_REG:	
		if (read_reg) 
			TMI_IND_R1(mgvbase, reg, val)
		else
			TMI_IND_W1(mgvbase, reg, val);
		break;
	   case CSC_REG:	
		if (read_reg) 
			CSC_IND_R1(mgvbase, reg, val)
		else
			CSC_IND_W1(mgvbase, reg, val);
		break;
	   default: 
		msg_printf(ERR, "Unknown register type\n");
		break;
	}
}
