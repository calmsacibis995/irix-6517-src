/***********************************************************************\
*	File:		nvram.c						*
*									*
*	Contains basic functions for reading, writing, and checking	*
*	the IO6 nvram. 							*
*									*
\***********************************************************************/

#include <sys/types.h>
#include <sys/cpu.h>
#include <sys/SN/nvram.h>
#include <libkl.h>
#include "ip27prom.h"
#include "pod.h"

/*
 * get_nvreg()
 *	Read a byte from the NVRAM.
 */
uint
get_nvreg(uint offset)
{
	printf("XXX fixme: get_nvreg\n");
	return 0;
}

/*
 * set_nvreg()
 *	Writes a byte into the NVRAM
 */
uint
set_nvreg(uint offset, uint byte)
{
	printf("XXX fixme: set_nvreg\n");
	return 0;
}

/*
 * nvchecksum()
 *	Calculates the checksum for the NVRAM attached to the master
 *	IO6.
 */

uint
nvchecksum(void)
{
#ifdef SABLE
    return 0;	/* XXX get_nvreg takes too long in Sable */
#else
    register uint nv_reg;
    unchar nv_val;
    signed char checksum = 0xa5;

    /*
     * do the checksum on all of the nvram, skip the checksum byte !!
     */
    for (nv_reg = 0; nv_reg < NVOFF_HIGHFREE; nv_reg++) {
	nv_val = get_nvreg(nv_reg);
	if (nv_reg != NVOFF_CHECKSUM && nv_reg != NVOFF_RESTART)
	   checksum ^= nv_val;

	/* following is a tricky way to rotate */
	checksum = (checksum << 1) | (checksum < 0);
    }

    return ((uint)checksum & 0xff);
#endif
}

/*
 * set_nvram
 *	Write the given string to nvram at an offset.  If operation
 *	fails, return -1, otherwise return 0.
 */

int
set_nvram(uint nv_off, uint nv_len, char *string)
{
    uint i;
    unchar new_cksum;

    if ((nv_off + nv_len) >= NVLEN_MAX)
	return -1;

    for (i = 0; i < nv_len; i++) {
	if (set_nvreg(i+nv_off, string[i]))
	    return -1;
    }

    /* Update the checksum */
    new_cksum = nvchecksum();
    if (set_nvreg(NVOFF_CHECKSUM, new_cksum))
	return -1;

    return 0;
}

/*
 * get_nvram
 *	Just reads a sequence of bytes out of the NVRAM.
 */

void
get_nvram(uint nv_off, uint nv_len, char buf[])
{
    uint i;

    /* Avoid overruns */
    if ((nv_off + nv_len) >= NVLEN_MAX) {
	*buf = '\0';
	return;
    }

    /* Transfer the bytes into the array */
    for (i = 0; i < nv_len; i++)
	*buf++ = get_nvreg(i+nv_off);

    *buf = '\0';
}


/*
 * nvram_okay
 *	Examines the NVRAM checksum and returns a value indicating
 *	whether the NVRAM is valid (1 == NVRAM valid, 0 == bogus).
 */

uint
nvram_okay(void)
{
    uint	cksum;
    uint	old_cksum;
    jmp_buf	fault_buf;
    void	*old_buf;

    /* Deal gracefully with broken IO6's */
    if (setfault(fault_buf, &old_buf))	{
	restorefault(old_buf);
	return 0;
    }

    cksum = nvchecksum();
    old_cksum = get_nvreg(NVOFF_CHECKSUM);

    restorefault(old_buf);
    return (cksum == old_cksum);
}
