/**************************************************************************
 *									  *
 * 		 Copyright (C) 1995, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident "$Revision: 1.14 $"

/*
 * nvram.c - nvram support
 */

#include <sys/types.h>
#include <sys/systm.h>
#include <sys/immu.h>
#include <sys/cmn_err.h>
#include <sys/sema.h>

#include <sys/arcs/types.h>
#include <sys/arcs/kerncb.h>

#include <sys/cpu.h>
#include <sys/ds1687clk.h>
#include <sys/RACER/IP30nvram.h>

#include "RACERkern.h"

/* Fetch the nvram table from the PROM.  The table is static because we
 * can not call kern_malloc yet.  We can at least tell if the actual table
 * is larger than we think by the return value.
 */
static struct nvram_entry nvram_tab[40];

void
load_nvram_tab(void)
{
	if (arcs_nvram_tab((char *)nvram_tab, sizeof(nvram_tab)) > 0)
		cmn_err(CE_WARN,"IP30 nvram table has grown!\n");
}

/*
 * nvchecksum calculates the checksum for the nvram, assuming we hold the
 * proper lock and the IOC3 timing is set up previously
 */
static uchar_t
nvchecksum(void)
{
	/*
	 * seed the checksum so all-zeroes (all-ones) nvram doesn't have a zero
	 * (all-ones) checksum
	 */
	uchar_t		checksum = 0xa5;
	int		i;

	/* checksum for the first 2 sets of nvram variables */
	WR_DS1687_RTC(RTC_CTRL_A, RTC_OSCILLATOR_ON);
	/* +1 to skip the checksum itself */
	for (i = NVRAM_SET_0_MIN+NVOFF_REVISION; i <= NVRAM_SET_1_MAX; i++) {
		checksum ^= RD_DS1687_RTC(i);
		checksum = (checksum << 1) | (checksum >> 7);
	}

	/* checksum for the last set of nvram variables */
	WR_DS1687_RTC(RTC_CTRL_A, RTC_SELECT_BANK_1 | RTC_OSCILLATOR_ON);
	for (i = NVRAM_SET_2_MIN; i <= NVRAM_SET_2_MAX; i++) {
		WR_DS1687_RTC(RTC_X_RAM_ADDR, i & 0x7f);
		checksum ^= RD_DS1687_RTC(RTC_X_RAM_DATA);
		checksum = (checksum << 1) | (checksum >> 7);
	}

	return checksum;
}

/* write string to non-volatile memory */
int
ip30_set_an_nvram(register int nv_off, register int nv_len, 
						register char *string)
{
	uchar_t		checksum;
	int		nv_last = nv_off + nv_len - 1;
	ioc3reg_t	old;
	int		s;

	if (nv_last >= NVLEN_MAX)
		return -1;

	nv_off += NVRAM_REG_OFFSET;
	nv_last += NVRAM_REG_OFFSET;

	s = mutex_spinlock_spl(&ip30clocklock, splprof);

	old = slow_ioc3_sio();

	/* access the first 2 sets of nvram variables through bank 0 */
	if (nv_off <= NVRAM_SET_1_MAX)
		WR_DS1687_RTC(RTC_CTRL_A, RTC_OSCILLATOR_ON);
	while (nv_off <= nv_last && nv_off <= NVRAM_SET_1_MAX)
		WR_DS1687_RTC(nv_off++, *string++);

	/* must access the last set of nvram variables through bank 1 */
	if (nv_off <= nv_last)
		WR_DS1687_RTC(RTC_CTRL_A,
			RTC_SELECT_BANK_1 | RTC_OSCILLATOR_ON);
	while (nv_off <= nv_last && nv_off <= NVRAM_SET_2_MAX) {
		WR_DS1687_RTC(RTC_X_RAM_ADDR, (nv_off++) & 0x7f);
		WR_DS1687_RTC(RTC_X_RAM_DATA, *string++);
	}

	checksum = nvchecksum();
	WR_DS1687_RTC(NVOFF_CHECKSUM + NVRAM_REG_OFFSET, checksum);

	restore_ioc3_sio(old);

	mutex_spinunlock(&ip30clocklock, s);

	return 0;
}


/*  Set nv ram variable name to value.  All the real work is done in
 * set_an_nvram().  There is no get_nvram because the code that
 * would use it always gets it from kopt_find instead.
 *
 * This is called from syssgi().
 *
 * Negative return values indicate failure.
 */
int
set_nvram(char *name, char *value)
{
	register struct nvram_entry *nvt;
	int valuelen = strlen(value);

	if(!strcmp("eaddr", name))
		return -2;

	/* Don't allow setting the password from Unix, only clearing. */
	if (!strcmp(name, "passwd_key") && valuelen)
		return -2;

	/* check to see if it is a valid nvram name */
	for(nvt = nvram_tab; nvt->nt_nvlen; nvt++)
		if(!strcmp(nvt->nt_name, name)) {
			int status;

			/* PDS variables are handled outside the kernel */
			if(NVOFF_PDSVAR == nvt->nt_nvaddr) {
				return -1;
			}

			if(valuelen > nvt->nt_nvlen)
				return NULL;
			if(valuelen < nvt->nt_nvlen)
				++valuelen;	/* write out NULL */

			status = ip30_set_an_nvram(nvt->nt_nvaddr, 
							valuelen, value);

			return status;
		}

	return -1;
}

/*
 * get the current value of a ip30 variable that is not available
 * via the nvram variables.
 *
 * this routine returns 
 *	0 success, with value in supplied buffer
 *     -1 failure, buffer is undefined
 */
ip30_get_an_nvram(register int nv_off, register int nv_len, 
						register char *buffer)
{
	int		nv_last = nv_off + nv_len - 1;
	ioc3reg_t	old;
	int		s;


	if (nv_last >= NVLEN_MAX)		/* sanity */
		return -1;

	nv_off += NVRAM_REG_OFFSET;
	nv_last += NVRAM_REG_OFFSET;

	s = mutex_spinlock_spl(&ip30clocklock, splprof);

	old = slow_ioc3_sio();

	/* access the first 2 sets of nvram variables through bank 0 */
	if (nv_off <= NVRAM_SET_1_MAX)
		WR_DS1687_RTC(RTC_CTRL_A, RTC_OSCILLATOR_ON);
	while (nv_off <= nv_last && nv_off <= NVRAM_SET_1_MAX)
		*buffer++ = RD_DS1687_RTC(nv_off++);

	/* must access the last set of nvram variables through bank 1 */
	if (nv_off <= nv_last)
		WR_DS1687_RTC(RTC_CTRL_A,
			RTC_SELECT_BANK_1 | RTC_OSCILLATOR_ON);
	while (nv_off <= nv_last && nv_off <= NVRAM_SET_2_MAX) {
		WR_DS1687_RTC(RTC_X_RAM_ADDR, (nv_off++) & 0x7f);
		*buffer++ = RD_DS1687_RTC(RTC_X_RAM_DATA);
	}

	restore_ioc3_sio(old);

	mutex_spinunlock(&ip30clocklock, s);

	return 0;
}
