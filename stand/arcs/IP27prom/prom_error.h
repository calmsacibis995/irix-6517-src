/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1992-1996, Silicon Graphics, Inc.          *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

/*
 * File: prom_error.h
 */

#ifndef	_PROM_ERROR_H_
#define	_PROM_ERROR_H_

#include <sys/SN/klconfig.h>

void error_save_reset(void);
void error_move_reset(void);
int error_log_reset(lboard_t *);
void error_clear_hub_regs(void);

void error_setup_pi_stack(nasid_t nasid);

void error_clear_io6(void);
void error_enable_checking(int, int, int);
void error_clear_hub_erreg(nasid_t);
void error_clear_exception(void);
void error_disable_checking(int, int, int);
void error_show_reset(void);

#define SYSAD_ALL	(PI_SYSAD_ERRCHK_ECCGEN | PI_SYSAD_ERRCHK_QUALGEN | \
			 PI_SYSAD_ERRCHK_SADP   | PI_SYSAD_ERRCHK_CMDP    | \
			 PI_SYSAD_ERRCHK_QUAL	| PI_SYSAD_ERRCHK_STATE)

#define ERROR_INTR	1


#endif /* _PROM_ERROR_H_ */
