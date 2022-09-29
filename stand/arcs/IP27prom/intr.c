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
 * File: intr.c
 *	Interrupt functionality for the prom.
 */
#include <sys/types.h>
#include <libkl.h>
#include <sys/SN/SN0/IP27.h>
#include "ip27prom.h"
#include "libasm.h"
#include "intr.h"
#include "libc.h"

typedef int	i_func(int);
typedef i_func	*i_func_t;

static struct cause_intr_s {
    i_func_t handler;
    char *intr;
} cause_intr_tbl[NUM_CAUSE_INTRS] = {
    { (i_func_t)intr_default_hndlr, "SRB_SWTIMO" },
    { (i_func_t)intr_default_hndlr, "SRB_NET"	 },
    { (i_func_t)intr_default_hndlr, "SRB_DEV0"	 },
    { (i_func_t)intr_default_hndlr, "SRB_DEV1"	 },
    { (i_func_t)intr_default_hndlr, "SRB_TIMOCLK"},
    { (i_func_t)intr_default_hndlr, "SRB_PROFCLK"},
    { (i_func_t)intr_pi_err_hndlr,  "SRB_ERR"	 },
    { (i_func_t)intr_default_hndlr, "SRB_SCHEDCLK"},
};


static struct eintr_action_s {
    i_func_t	handler;
    char *error;
} eintr_action_table[] = {
    { intr_fatal_action,	"PI_ERR_SPOOL_CMP_B"  },
    { intr_fatal_action,	"PI_ERR_SPOOL_CMP_A"  },
    { intr_fatal_action,	"PI_ERR_SPUR_MSG_B"   },
    { intr_fatal_action,	"PI_ERR_SPUR_MSG_A"   },
    { intr_fatal_action,	"PI_ERR_WRB_TERR_B"   },
    { intr_fatal_action,	"PI_ERR_WRB_TERR_A"   },
    { intr_fatal_action,	"PI_ERR_WRB_WERR_B"   },
    { intr_fatal_action,	"PI_ERR_WRB_WERR_A"   },
    { intr_fatal_action,	"PI_ERR_SYSSTATE_B"   },
    { intr_fatal_action,	"PI_ERR_SYSSTATE_A"   },
    { intr_fatal_action,	"PI_ERR_SYSAD_DATA_B" },
    { intr_fatal_action,	"PI_ERR_SYSAD_DATA_A" },
    { intr_fatal_action,	"PI_ERR_SYSAD_ADDR_B" },
    { intr_fatal_action,	"PI_ERR_SYSAD_ADDR_A" },
    { intr_fatal_action,	"PI_ERR_SYSCMD_DATA_B"},
    { intr_fatal_action,	"PI_ERR_SYSCMD_DATA_A"},
    { intr_fatal_action,	"PI_ERR_SYSCMD_ADDR_B"},
    { intr_fatal_action,	"PI_ERR_SYSCMD_ADDR_A"},
    { intr_fatal_action,	"PI_ERR_BAD_SPOOL_B"  },
    { intr_fatal_action,	"PI_ERR_BAD_SPOOL_A"  },
    { intr_fatal_action,	"PI_ERR_UNCAC_UNCORR_B"},
    { intr_fatal_action,	"PI_ERR_UNCAC_UNCORR_A"},
    { intr_fatal_action,	"PI_ERR_SYSSTATE_TAG_B"},
    { intr_fatal_action,	"PI_ERR_SYSSTATE_TAG_A"},
    { intr_fatal_action,	"PI_ERR_MD_UNCORR"   },
};


void
intr_disable(void)
{
    set_cop0(C0_SR, (get_cop0(C0_SR) & ~SR_IE));
}

int
intr_process(void)
{
    int	irc;		/* interrupt return code */
    unsigned int cause, sr;
    unsigned int intr_bits, intr_ndx;

    cause = get_cop0(C0_CAUSE);
    sr	  = get_cop0(C0_SR);

    if (sr & SR_IE == 0) return -1;

    if ((intr_bits = (cause & sr & SR_IMASK)) == 0)
	return -1;

    if (intr_bits & SRB_ERR)
	intr_ndx = SRB_ERR_IDX;
    else if (intr_bits & SRB_DEV1)
	intr_ndx = SRB_DEV1_IDX;
    else if (intr_bits & SRB_PROFCLK)
	intr_ndx = SRB_PROFCLK_IDX;
    else if (cause & SRB_SCHEDCLK)
	intr_ndx = SRB_SCHEDCLK_IDX;
    else if (cause & SRB_TIMOCLK)
	    intr_ndx = SRB_TIMOCLK_IDX;
    else if (cause & SRB_SWTIMO)
	    intr_ndx = SRB_SWTIMO_IDX;
    else if (cause & SRB_DEV0)
	    intr_ndx = SRB_DEV0_IDX;
    else if (cause & SRB_NET)
	intr_ndx = SRB_NET_IDX;

    irc = cause_intr_tbl[intr_ndx].handler(intr_ndx);
    if (irc)
	intr_disable();
    return irc;
}


int
intr_default_hndlr(int level)
{
    printf("Interrupt Level %d (%s) received\n", level,
	   cause_intr_tbl[level].intr);
    return -1;
}

static int
find_low_bitset(hubreg_t reg)
{
    int bit = 0;

    while (!(reg & 1)) {
	bit++;
	reg >>= 1;
    }

    return bit;
}


int
intr_pi_err_hndlr(int level)
{
    hubreg_t	eint_pend;
    int eint_bit;
    int rc = 0;
    int eint_result;
    char buf[8];

    printf("Interrupt %d received (%s)\n", level, cause_intr_tbl[level].intr);

    eint_pend = LD(LOCAL_HUB(PI_ERR_INT_PEND));
    if (eint_pend == 0) {
	printf("Spurious error interrupt? PI_ERR_INT_PEND = 0\n");
	return 0;
    }

    while (eint_pend) {
	eint_bit = find_low_bitset(eint_pend);
	eint_result = INTR_FATAL;
	if (eintr_action_table[eint_bit].handler)
	    eint_result = eintr_action_table[eint_bit].handler(eint_bit);

	switch (eint_result) {
	case INTR_IGNORE:
	    printf("Ignoring error %s in PI_ERR_INT_PEND\n",
		   eintr_action_table[eint_bit].error);
	    eint_pend &= ~(1L << eint_bit);
	    break;

	case INTR_FATAL:
	default:
	    printf("Fatal error interrupt %s in PI_ERR_INT_PEND\n",
		   eintr_action_table[eint_bit].error);
	    eint_pend &= ~(1L << eint_bit);
	    rc = -1;
	}
    }

    return rc;
}


int
intr_clear_md_error(int eint_bit)
{
    LD(LOCAL_HUB(MD_DIR_ERROR_CLR));
    LD(LOCAL_HUB(MD_MEM_ERROR_CLR));
    LD(LOCAL_HUB(MD_PROTOCOL_ERROR_CLR));
    LD(LOCAL_HUB(MD_MISC_ERROR_CLR));

    return INTR_IGNORE;
}

int
intr_ignore_action(int eint_bit)
{

    *LOCAL_HUB(PI_ERR_INT_PEND) |= (1L << eint_bit);
    return INTR_IGNORE;
}

int
intr_fatal_action(int eint_bit)
{
    return INTR_FATAL;
}


void
enable_cpu_intrs(void)
{
	set_cop0(C0_SR, (get_cop0(C0_SR) | PROM_SR_INTR));
}

void
disable_cpu_intrs(void)
{
	set_cop0(C0_SR, (get_cop0(C0_SR) & ~PROM_SR_INTR));
}
