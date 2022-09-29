/**************************************************************************
 *									  *
 * 		 Copyright (C) 1996 Silicon Graphics, Inc.	  	  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident "$Revision: 1.4 $"

#ifndef	_REPORT_H_
#define _REPORT_H_	

#define TESTNAME_WIDTH		14
#define COMMENT_WIDTH		26
#define CODE_TYPE_WIDTH		4
#define STATUS_WIDTH		14
#define PART_NUM_WIDTH		12
#define SNUM_WIDTH		12
#define NIC_NUM_WIDTH		12
#define ERRSTAT_WIDTH		4
#define CHIP_WIDTH		8

void	result_pass(char *test_name, int diag_mode);
void	result_fail(char *test_name, int diag_rc, char *comment);	
void	result_diagnosis(char *test_name, ...);	
void	result_code(char *test_name, char *code_type, ...);
int     make_diag_rc(short diag_val, short diag_parm);

#endif /* _REPORT_H_ */
