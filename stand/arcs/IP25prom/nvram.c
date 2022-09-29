/***********************************************************************\
*	File:		nvram.c						*
*									*
*	Contains basic functions for reading, writing, and checking	*
*	the IO4 nvram.  Need to pass the EPC_REGION and adapter number.	* 
*									*
\***********************************************************************/

#include <sys/types.h>
#include <sys/cpu.h>
#include <sys/EVEREST/epc.h>
#include <sys/EVEREST/nvram.h>
#include "ip25prom.h"
#include "prom_externs.h"
#include <setjmp.h>

/*
 * Get NVRAM values.  These routines are endian-independent. 
 */
#define ADDR(_x) \
	(SWIN_BASE(EPC_REGION, master_epc_adap()) + EPC_NVRTC + (_x) + \
	 7 * (!getendian()))

#define NVRAM_GET(_reg) \
	( *((unsigned char *) ADDR(_reg)))
#define NVRAM_SET(_reg, _value) \
	NVRAM_GET(_reg) = (_value);

/*
 * get_nvreg()
 *	Read a byte from the NVRAM.
 */
/*ARGSUSED*/
uint
get_nvreg(uint offset)
{
#ifdef SABLE
    return 0;
#else
    uint value;

    /* First set up the XRAM Page register */
    NVRAM_SET(NVR_XRAMPAGE, XRAM_PAGE(offset));
    value = (uint) NVRAM_GET(XRAM_REG(offset));	
    return value;
#endif
}


/*
 * set_nvreg()
 *	Writes a byte into the NVRAM
 */
/*ARGSUSED*/
uint
set_nvreg(uint offset, uint byte)
{
    uint value; 

#ifdef SABLE
    value = byte;
#else
    /* First set up the XRAM Page register */
    NVRAM_SET(NVR_XRAMPAGE, XRAM_PAGE(offset));
    NVRAM_SET(XRAM_REG(offset), byte);  
    value = NVRAM_GET(XRAM_REG(offset));
#endif

    if (value == byte)
	return 0;
    else
	return 1;	
}


/*
 * nvchecksum()
 *	Calculates the checksum for the NVRAM attached to the master
 *	IO4 EPC.
 */

uint 
nvchecksum(void)
{
    register uint nv_reg;
    unchar nv_val;
    signed char checksum = 0xa5;

    /*
     * do the checksum on all of the nvram, skip the checksum byte !!
     */
    for (nv_reg = 0; nv_reg < NVOFF_HIGHFREE; nv_reg++) {
        nv_val = get_nvreg(nv_reg);
	if (nv_reg != NVOFF_NEW_CHECKSUM &&
	    nv_reg != NVOFF_RESTART)
           checksum ^= nv_val;

        /* following is a tricky way to rotate */
        checksum = (checksum << 1) | (checksum < 0);
    }

    return ((uint)checksum & 0xff);
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
    if (set_nvreg(NVOFF_NEW_CHECKSUM, new_cksum))
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
    unsigned	*old_buf;

#ifdef	SABLE
    return 1;
#else
    /* Deal gracefully with broken IO4s's */
    if (setfault(fault_buf, &old_buf))  {
	restorefault(old_buf);
	return 0;
    }

    cksum = nvchecksum();
    old_cksum = get_nvreg(NVOFF_NEW_CHECKSUM);

    restorefault(old_buf);
    return (cksum == old_cksum);
#endif
}
