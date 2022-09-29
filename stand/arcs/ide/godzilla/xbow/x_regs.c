/*
 * x_regs.c : 
 *	Xbow Register read/write Tests
 *
 * Copyright 1995, Silicon Graphics, Inc.
 * ALL RIGHTS RESERVED
 *
 * UNPUBLISHED -- Rights reserved under the copyright laws of the United
 * States.   Use of a copyright notice is precautionary only and does not
 * imply publication or disclosure.
 *
 * U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the Rights
 * in Technical Data and Computer Software clause at DFARS 252.227-7013 and/or
 * in similar or successor clauses in the FAR, or the DOD or NASA FAR
 * Supplement.  Contractor/manufacturer is Silicon Graphics, Inc.,
 * 2011 N. Shoreline Blvd. Mountain View, CA 94039-7311.
 *
 * THE CONTENT OF THIS WORK CONTAINS CONFIDENTIAL AND PROPRIETARY
 * INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,
 * DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS STRICTLY
 * PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON
 * GRAPHICS, INC.
 */

/* NOTE: partially run on sable, but the default values (in d_xbow.h) 
 *	need to be adjusted. XXX
 * Only the crossbow registers are tested. 
 * The link registers are read to give system status.
 */
#ident "ide/godzilla/xbow/x_regs.c:  $Revision: 1.12 $"

/*
 * x_regs.c - Xbow Register read/write Tests
 */
#include "sys/types.h"
#include "sys/cpu.h"
#include "sys/sbd.h"
#include "libsk.h"
#include "libsc.h"
#include "uif.h"
#include "d_godzilla.h"
#include "d_frus.h"
#include "d_xbow.h"
#include "sys/xtalk/xbow.h"
#include "d_prototypes.h"

/*
 * Forward References 
 */
bool_t x_regs(void);
bool_t _x_regs_read_write(Xbow_Regs *regs_ptr);
bool_t _special_x_cases(void);
bool_t x_regs_dump(void);


Xbow_Regs	gz_xbow_regs[] = {
/*
 * crossbow registers
 */
{"XBOW_WID_ID",			XBOW_WID_ID,		GZ_READ_ONLY,
  XBOW_WID_ID_MASK},

{"XBOW_WID_STAT",		XBOW_WID_STAT,		GZ_READ_ONLY,
  XBOW_WID_STAT_MASK},

{"XBOW_WID_ERR_UPPER",		XBOW_WID_ERR_UPPER,	GZ_READ_ONLY,
  XBOW_WID_ERR_UPPER_MASK},

{"XBOW_WID_ERR_LOWER",		XBOW_WID_ERR_LOWER,	GZ_READ_ONLY,
  XBOW_WID_ERR_LOWER_MASK},

{"XBOW_WID_CONTROL",		XBOW_WID_CONTROL,	GZ_READ_WRITE,
  XBOW_WID_CONTROL_MASK},

{"XBOW_WID_REQ_TO",		XBOW_WID_REQ_TO,	GZ_READ_WRITE,
  XBOW_WID_REQ_TO_MASK},

{"XBOW_WID_INT_UPPER",		XBOW_WID_INT_UPPER,	GZ_READ_WRITE,
  XBOW_WID_INT_UPPER_MASK},

{"XBOW_WID_INT_LOWER",		XBOW_WID_INT_LOWER,	GZ_READ_WRITE,
  XBOW_WID_INT_LOWER_MASK},

/* writing to this register clears it, regardless of the written value */
{"XBOW_WID_ERR_CMDWORD",	XBOW_WID_ERR_CMDWORD,	GZ_READ_ONLY,
  XBOW_WID_ERR_CMDWORD_MASK},

{"XBOW_WID_LLP",		XBOW_WID_LLP,		GZ_READ_WRITE,
  XBOW_WID_LLP_MASK},

/* XBOW_WID_STAT_CLR is not tested as reading it clears it */

{"XBOW_WID_ARB_RELOAD",		XBOW_WID_ARB_RELOAD,	GZ_READ_WRITE,
  XBOW_WID_ARB_RELOAD_MASK},

/* 2 perf counters, according to released specs 9.0 */
{"XBOW_WID_PERF_CTR_A",		XBOW_WID_PERF_CTR_A,	GZ_READ_ONLY,
  XBOW_WID_PERF_CTR_A_MASK},

{"XBOW_WID_PERF_CTR_B",		XBOW_WID_PERF_CTR_B,	GZ_READ_ONLY,
  XBOW_WID_PERF_CTR_B_MASK},

{"XBOW_WID_NIC",		XBOW_WID_NIC,		GZ_READ_ONLY,
  XBOW_WID_NIC_MASK},
{"", -1, -1, -1}
};

Xbow_Regs	gz_xbow_link_regs[] = {
/* 
 * link registers: only link 8, 9, f for now
 */
/* link 8 */
{"XB_LINK_IBUF_FLUSH(XBOW_PORT_8)", XB_LINK_IBUF_FLUSH(XBOW_PORT_8), GZ_READ_ONLY,
  XB_LINK_IBUF_FLUSH_MASK},

{"XB_LINK_CTRL(XBOW_PORT_8)",	XB_LINK_CTRL(XBOW_PORT_8), GZ_READ_WRITE,
  XB_LINK_CTRL_MASK},

{"XB_LINK_STATUS(XBOW_PORT_8)",	XB_LINK_STATUS(XBOW_PORT_8), GZ_READ_ONLY,
  XB_LINK_STATUS_MASK},

{"XB_LINK_AUX_STATUS(XBOW_PORT_8)", XB_LINK_AUX_STATUS(XBOW_PORT_8), GZ_READ_ONLY,
  XB_LINK_AUX_STATUS_MASK},

{"XB_LINK_ARB_UPPER(XBOW_PORT_8)", XB_LINK_ARB_UPPER(XBOW_PORT_8), GZ_READ_WRITE,
  XB_LINK_ARB_UPPER_MASK},

{"XB_LINK_ARB_LOWER(XBOW_PORT_8)", XB_LINK_ARB_LOWER(XBOW_PORT_8), GZ_READ_WRITE,
  XB_LINK_ARB_LOWER_MASK},

/* do not test XB_LINK_STATUS_CLR as it has side effects (reading clears */
/*	XB_LINK_STATUS and XB_LINK_AUX_STATUS) */

/* do not test XB_LINK_RESET as it has side effects */
/*	(writing resets the Xbow LLP's) */

/* link 9 */
{"XB_LINK_IBUF_FLUSH(XBOW_PORT_9)", XB_LINK_IBUF_FLUSH(XBOW_PORT_9), GZ_READ_ONLY,
  XB_LINK_IBUF_FLUSH_MASK},

{"XB_LINK_CTRL(XBOW_PORT_9)",	XB_LINK_CTRL(XBOW_PORT_9), GZ_READ_WRITE,
  XB_LINK_CTRL_MASK},

{"XB_LINK_STATUS(XBOW_PORT_9)",	XB_LINK_STATUS(XBOW_PORT_9), GZ_READ_ONLY,
  XB_LINK_STATUS_MASK},

{"XB_LINK_AUX_STATUS(XBOW_PORT_9)", XB_LINK_AUX_STATUS(XBOW_PORT_9), GZ_READ_ONLY,
  XB_LINK_AUX_STATUS_MASK},

{"XB_LINK_ARB_UPPER(XBOW_PORT_9)", XB_LINK_ARB_UPPER(XBOW_PORT_9), GZ_READ_WRITE,
  XB_LINK_ARB_UPPER_MASK},

{"XB_LINK_ARB_LOWER(XBOW_PORT_9)", XB_LINK_ARB_LOWER(XBOW_PORT_9), GZ_READ_WRITE,
  XB_LINK_ARB_LOWER_MASK},

/* link f */
{"XB_LINK_IBUF_FLUSH(XBOW_PORT_F)", XB_LINK_IBUF_FLUSH(XBOW_PORT_F), GZ_READ_ONLY,
  XB_LINK_IBUF_FLUSH_MASK},

{"XB_LINK_CTRL(XBOW_PORT_F)",	XB_LINK_CTRL(XBOW_PORT_F), GZ_READ_WRITE,
  XB_LINK_CTRL_MASK},

{"XB_LINK_STATUS(XBOW_PORT_F)",	XB_LINK_STATUS(XBOW_PORT_F), GZ_READ_ONLY,
  XB_LINK_STATUS_MASK},

{"XB_LINK_AUX_STATUS(XBOW_PORT_F)", XB_LINK_AUX_STATUS(XBOW_PORT_F), GZ_READ_ONLY,
  XB_LINK_AUX_STATUS_MASK},

{"XB_LINK_ARB_UPPER(XBOW_PORT_F)", XB_LINK_ARB_UPPER(XBOW_PORT_F), GZ_READ_WRITE,
  XB_LINK_ARB_UPPER_MASK},

{"XB_LINK_ARB_LOWER(XBOW_PORT_F)", XB_LINK_ARB_LOWER(XBOW_PORT_F), GZ_READ_WRITE,
  XB_LINK_ARB_LOWER_MASK},

{"", -1, -1, -1}

};

/*
 * Name:	x_regs.c
 * Description:	tests registers in the Xbow
 * Input:	none
 * Output:	Returns 0 if no error, 1 if error
 * Error Handling: 
 * Side Effects: 
 * Remarks:     
 * Debug Status: compiles, simulated, not emulated
 */
bool_t
x_regs(void)
{
	Xbow_Regs	*regs_ptr;

	/* Xbow registers */
	regs_ptr = gz_xbow_regs;
	d_errors = 0;
	while (regs_ptr->name[0] != NULL) {
	    if (regs_ptr->mode == GZ_READ_WRITE) {
		if (_x_regs_read_write(regs_ptr)) {
		    goto _error;
		}
	    }
	    /* GZ_READ_ONLY, GZ_WRITE_ONLY, GZ_NO_ACCESS */
	    regs_ptr++;
	}
	/* Xbow link registers */
	regs_ptr = gz_xbow_link_regs;
	d_errors = 0;
	while (regs_ptr->name[0] != NULL) {
	   if (regs_ptr->mode == GZ_READ_WRITE) {
		if (_x_regs_read_write(regs_ptr)) {
		    goto _error;
		}
	   }
	   /* GZ_READ_ONLY GZ_WRITE_ONLY GZ_NO_ACCESS cases now */
	   regs_ptr++;
	}
	if (_special_x_cases()) { 
		d_errors++;
	}
_error:
#ifdef NOTNOW
	REPORT_PASS_OR_FAIL(d_errors, "Xbow Read-Write Register", D_FRU_FRONT_PLANE);
#endif
	REPORT_PASS_OR_FAIL( &CPU_Test_err[CPU_ERR_XBOW_0001], d_errors );
}

/*
 * Name:	_x_regs_read_write.c
 * Description:	Performs Write/Reads on the Xbow Registers
 * Input:	Pointer to the register structure
 * Output:	Returns 0 if no error, 1 if error
 * Error Handling: 
 * Side Effects: 
 * Remarks:     
 * Debug Status: compiles, simulated, not emulated
 */
bool_t
_x_regs_read_write(Xbow_Regs *regs_ptr)
{
	xbowreg_t	saved_reg_val, write_val, read_val;

	write_val = 0x5a5a5a5a;	/* XXX: Some value */
	/* try walking bit in emulation XXX */

	/* Save the current value */
	XB_REG_RD_32(regs_ptr->address, (xbowreg_t)~0x0, saved_reg_val);

	/* Write the test value */
	XB_REG_WR_32(regs_ptr->address, regs_ptr->mask, write_val);

	/* Read back the register */
	XB_REG_RD_32(regs_ptr->address, regs_ptr->mask, read_val);

	/* Compare the expected and received values */
	if ((write_val & regs_ptr->mask) != read_val) {
	   msg_printf(SUM, "\nERROR Read/Write: %s; Addr = 0x%x; Mask = 0x%x; \
\n\t\t Exp = 0x%x; Rcv = 0x%x\n", 
		regs_ptr->name, regs_ptr->address, regs_ptr->mask, 
		(write_val&regs_ptr->mask), read_val);
	   d_errors++;
	   if (d_exit_on_error) {
		return(1);
	   }
	}
	else msg_printf(DBG, "Write/Read OK.\n");

	/* Restore the register with old value */
	XB_REG_WR_32(regs_ptr->address, (xbowreg_t)~0x0, saved_reg_val);

	return(0);
}

/*
 * Name:	_special_x_cases
 * Description:	handles special information about some registers
 * Input:	none
 * Output:	Returns 0 if no error, 1 if error
 * Error Handling: 
 * Side Effects: none
 * Remarks:     
 * Debug Status: compiles, simulated, not emulated
 */
bool_t
_special_x_cases(void)
{
	xbowreg_t	rev_num;
	xbowreg_t	link_present;
	xbowreg_t	alive; 
	char		index;

	msg_printf(INFO,"Misc xbow register info:\n");

	/* read xbow revision number. if 0, print error, else print rev */
	XB_REG_RD_32(XBOW_WID_ID, D_XBOW_WID_ID_REV_NUM_MASK, rev_num);
	if (rev_num == 0) return (1);
	else msg_printf(INFO,"\t\twidget revision number %x\n", 0xf & (rev_num >> 28));

	/* read relevant info in the Xbow link registers */
	for (index = XBOW_PORT_8; index <= XBOW_PORT_F; index++) {
		XB_REG_RD_32(XB_LINK_AUX_STATUS(index), XB_AUX_STAT_PRESENT,
					link_present);
		XB_REG_RD_32(XB_LINK_STATUS(index), XB_STAT_LINKALIVE,
					alive);
		if (link_present && alive) 
			msg_printf(INFO,"\t\twidget %1x present & alive\n",
					index);
		else if (link_present)
			msg_printf(INFO,"\t\twidget %1x present\n",
					index);
		else if (alive)
			msg_printf(INFO,"\t\twidget %1x alive\n",
					index);
	}

	return (0);

}
/* Dumping out XB register contents */
bool_t
x_regs_dump(void)
{
	Xbow_Regs       *regs_ptr = gz_xbow_regs;
	xbowreg_t       reg_val;
	msg_printf(SUM,"Xbow Registers:\n");
	while (regs_ptr->name[0] != NULL) {
		XB_REG_RD_32(regs_ptr->address, regs_ptr->mask, reg_val);
		msg_printf(SUM,"%s \t\t:\t0x%x\n",regs_ptr->name,reg_val);
		regs_ptr++;
	}
	return(0);
}

