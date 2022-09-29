#ident	"$Id: msgs.s,v 1.1 1994/07/21 00:18:29 davidl Exp $"
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

/*
 * GENERIC and COMMON MESSAGES  
 */

/*
 * Macro to declare a message string
 */

#define DECL_MSG(name, msgstr)  \
	.globl	name;		\
name: 	.asciiz msgstr


	.data

DECL_MSG(_msg1, "\r\nAddress: 0x")
DECL_MSG(_msg2, " Expected: 0x")
DECL_MSG(_msg3, " Actual: 0x")
DECL_MSG(_msg4, " Xor: 0x")
DECL_MSG(_msg5,	"\r\nParity error on lw dcache: Addr= ")
DECL_MSG(_msg6,	"\r\nParity error on lcache: Addr= ")
DECL_MSG(_msg7,	" Data= ")
DECL_MSG(_msg8, " C0_SR= ")
DECL_MSG(_msg9,	" C0_ERROR= ")

DECL_MSG(_crlf, "\n\r")
DECL_MSG(msg_none,"None")
DECL_MSG(done_msg, " Completed\r\n")
DECL_MSG(failed_msg, " Failed\r\n")
DECL_MSG(pass_msg, "...PASSED ")
DECL_MSG(fail_msg, "...FAILED ")
DECL_MSG(xfailed_msg, "Test FAILED, Pass count: ")
DECL_MSG(success_msg, "Test PASSED, Pass count: ")
DECL_MSG(err_count_msg, ", Error count: ")

DECL_MSG(hang_msg, "\r\nPress the reset button to restart.\r\n")
DECL_MSG(cmd_errmsg, "\r\nInvalid parameters\r\n")


/*
 * Status on Menu Display
 */

/* Left column parameters */
DECL_MSG(msg_em, "ERROR_MODE (em)  = ")
DECL_MSG(msg_pm, "C0_ERROR_I (pm)  = 0x")
DECL_MSG(msg_fa, "FIRST_ADDR (fa)  = 0x")
DECL_MSG(msg_la, "LAST_ADDR  (la)  = 0x")
DECL_MSG(msg_ecc,"MEMORY_ECC (ecc) = ")
DECL_MSG(msg_c0sr,"C0_STATUS = 0x")

/* Right column parameters */
DECL_MSG(msg_el, "ERROR_LIMIT (el) = ")
DECL_MSG(msg_ec, "ERROR_COUNT      = ")
DECL_MSG(msg_ll, "LOOP_LIMIT  (ll) = ")
DECL_MSG(msg_pc, "PASS_COUNT       = ")
DECL_MSG(msg_configreg,"DiagMon Config/Status = 0x")

/* end */
