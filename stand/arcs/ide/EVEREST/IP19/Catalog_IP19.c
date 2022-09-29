/*
 *
 * Copyright 1991, Silicon Graphics, Inc.
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Silicon Graphics, Inc.
 *
 * RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
 * and Computer Software clause at DFARS 252.227-7013, and/or in similar or
 * successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -
 * rights reserved under the Copyright Laws of the United States.
 */

/***********************************************************************\
*	File:		Catalog_IP19.c					*
*									*
\***********************************************************************/

#include <sys/types.h>
#include <everr_hints.h>
#include <ide_msg.h>
#include <prototypes.h>

#ident	"arcs/ide/EVEREST/IP19/Catalog_IP19.c $Revision: 1.9 $"

void IP19_printf(void *);

/* Hints should have ALL 0s as the last entry in the table */
hint_t IP19_hints [] = {
{LOCAL_REGERR, "local_regtest", "Local register %s R/W error : Wrote 0x%llx Read 0x%llx\n", 0},
{BUSTAG_ERR, "bustags_reg", "Bus tag addr 0x%x R/W error : Wrote 0x%x Read 0x%x\n", 0},
{CONFIG_ERR, "cfig_regtest", "Configuration register %s R/W error : Wrote 0x%llx Read 0x%llx\n", 0},
{COMPARE_DERR, "counter", "Compare register data error : Expected 0x%x Got 0x%x\n", 0},
{COUNTER_BAD, "counter", "Incorrect contents in count register : Expected 0x%x Got 0x%x\n", 0},
{COUNTER_PINT, "counter", "Phantom count/compare interrupt received\n", 0},
{COUNTER_NOINT, "counter", "No count/compare interrupt received : Count 0x%x Compare 0x%x\n", 0},
{INT_L0_IP, "intr_level0", "Level 0 interrupt pending failure : Priority 0x%x IP0 0x%llx IP1 0x%llx\n", 0},
{INT_L0_HPIL, "intr_level0", "Level 0 highest priority interrupt level failure : HPIL 0x%llx\n", 0},
{INT_L0_CAUSE, "intr_level0", "Level 0 interrupt not indicated in Cause register 0x%x\n", 0},
{INT_L0_IPCLR, "intr_level0", "Level 0 interrupt pending not cleared : IP0 0x%llx IP1 0x%llx\n", 0},
{INT_L0_HPILCLR, "intr_level0", "Level 0 highest priority interrupt level not cleared : HPIL 0x%llx\n", 0},
{INT_L0_CAUSECLR, "intr_level0", "Level 0 interrupt pending not cleared in Cause register : Cause 0x%x\n", 0},
{INT_L0_CEL, "intr_level0", "Level 0 current exec level mismatch : Wrote 0x%llx Read 0x%llx\n", 0},
{INT_L0_CELHI, "intr_level0", "Level 0 interrupt not detected when priority >= CEL : Cause 0x%x\n", 0},
{INT_L0_CELLO, "intr_level0", "Level 0 interrupt detected when priority < CEL : Cause 0x%x\n", 0},
{INT_L0_CELCLR, "intr_level0", "Level 0 interrupt pending not cleared : Cause 0x%x\n", 0},
{INT_L0_MULT, "intr_level0", "Level 0 highest priority interrupt level incorrect : Expected 0x7f Got 0x%llx\n", 0},
{INT_L0_MULTIP0, "intr_level0", "Level 0 multiple interrupt pending incorrectly indicated : Expected 0x6000000000000009 Got 0x%llx\n", 0},
{INT_L0_MULTIP1, "intr_level0", "Level 0 multiple interrupt pending incorrectly indicated : Expected 0x9000000000000006 Got 0x%llx\n", 0},
{INT_L0_MULTCL0, "intr_level0", "Level 0 multiple interrupt pending not cleared : IP0 0x%llx\n", 0},
{INT_L0_MULTCL1, "intr_level0", "Level 0 multiple interrupt pending not cleared : IP1 0x%llx\n", 0},
{INT_L0_MULTCLH, "intr_level0", "Level 0 multiple interrupt HPIL not cleared : HPIL 0x%llx\n", 0},
{INT_L0_MULTCLC, "intr_level0", "Level 0 multiple interrupt Cause not cleared : Cause 0x%x\n", 0},
{INT_L0_NOINT, "intr_level0", "Level 0 interrupt did not occur : Priority 0x%x\n", 0},
{INT_L3_IP6, "intr_level3", "Level 3 interrupt pending not detected in CAUSE\n", 0},
{INT_L3_ERTOIP, "intr_level3", "Interrupting error not detected in ERTOIP\n", 0},
{INT_L3_IP6NC, "intr_level3", "Level 3 interrupt pending not cleared in Cause : Cause 0x%x\n", 0},
{INT_L3_CERTOIP, "intr_level3", "ERTOIP not cleared via write to CERTOIP : ERTOIP 0xllx\n", 0},
{INT_L3_NOINT, "intr_level3", "Level 3 interrupt did not occur : ERTOIP 0x%llx\n", 0},
{TIMER_CAUSECLR, "intr_timer", "Timer interrupt pending not cleared in Cause register\n", 0},
{TIMER_INV_INT, "intr_timer", "Invalid timer interrupt occurred\n", 0},
{TIMER_NOINT, "intr_timer", "Interval timer interrupt did not occur\n", 0},
{GROUP_IP3NC, "intr_group", "Group interrupt pending not cleared in Cause : Cause 0x%x\n", 0},
{GROUP_IP, "intr_group", "Group interrupt pending not set correctly in EV_IP0 : Expected 0x%llx Got 0x%llx\n", 0},
{GROUP_HPIL, "intr_group", "Group highest priority interrupt level failure : HPIL 0x%llx\n", 0},
{GROUP_CAUSE, "intr_group", "Group interrupt not indicated in Cause register 0x%x\n", 0},
{GROUP_IPCLR, "intr_group", "Group interrupt pending not cleared : IP0 0x%llx IP1 0x%llx\n", 0},
{GROUP_HPILCLR, "intr_group", "Group highest priority interrupt level not cleared : HPIL 0x%llx\n", 0},
{GROUP_CAUSECLR, "intr_group", "Group interrupt pending not cleared in Cause register : Cause 0x%x\n", 0},
{GROUP_NOINT, "intr_group", "Group interrupt did not occur : group 0x%x priority 0x%x\n", 0},
{WG_CMDERR, "wr_gatherer", "Write gatherer command only write : addr 0x%x expected 0x%x got 0x%x\n", 0},
{WG_MIXERR, "wr_gatherer", "Write gatherer mixed write : addr 0x%x expected 0x%x got 0x%x\n", 0},
{WG_DATAERR, "wr_gatherer", "Write gatherer data only write : addr 0x%x expected 0x%x got 0x%x\n", 0},
{(uint)0,  (caddr_t)0,  (caddr_t)0, (caddr_t)0}
};


uint	IP19_hintnum[] = { IP19_R4K, IP19_A, IP19_D, IP19_CC, 0 };

catfunc_t IP19_catfunc = { IP19_hintnum, IP19_printf  };
catalog_t IP19_catalog = { &IP19_catfunc, IP19_hints };

void
IP19_printf(void *loc)
{
	int *l = (int *)loc;
	msg_printf(ERR, "(IP19 slot:%d cpu:%d)\n", *l, *(l+1));
}
