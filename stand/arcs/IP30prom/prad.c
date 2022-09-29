/**************************************************************************
 *
 *  File:  prad.c
 *
 *  Description:
 *
 *      RAD audio support for the boot tune etc...
 *
 **************************************************************************
 *          Copyright (C) 1995 Silicon Graphics, Inc.
 *
 *  These coded instructions, statements, and computer programs  contain
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and
 *  are protected by Federal copyright law.  They  may  not be disclosed
 *  to  third  parties  or copied or duplicated in any form, in whole or
 *  in part, without the prior written consent of Silicon Graphics, Inc.
 *
 *
 **************************************************************************/
/*
 * $Id: prad.c,v 1.11 1999/12/01 21:12:59 mnicholson Exp $
 */

#if !NOGFX && !NOBOOTTUNE

#include "sys/types.h"
#include <sys/param.h>
#include <sys/sbd.h>
#include <sys/cpu.h>
#include <sys/invent.h>
#include <arcs/hinv.h>
#include <libsc.h>
#include <libsk.h>
#include "sys/rad.h"
#include <sys/RACER/IP30nvram.h>


#define RAD_CFG_WRITE(Base,Off,Val)	*(unsigned int*)(Base+Off) = Val
#define RAD_WRITE(Base,Off,Val) 	*(unsigned int*)(Base+Off) = Val
#define RAD_CFG_READ(Base,Off) 		*(unsigned int*)(Base+Off)
#define RAD_READ(Base,Off)		*(unsigned int*)(Base+Off)

#define RAD_CACHE_LINE_WORD_SIZE        32

typedef volatile char rad_reg_t;

/*
 * SETBITS sets a bitfield in a specified variable (var), starting at a specified
 * bit position (shift), using a specified mask (msk) to a specified value (val)
 */

#define SETBITS(var, msk, val, shift)           \
    (var) = ((var) & (~((msk) << (shift)))) | ((val) << (shift))

/*
 * local globals
 */

static rad_reg_t	*pcfgbase;
static rad_reg_t	*pbase;

#if HEART_COHERENCY_WAR
#define __dcache_wb_inval(a, l)		heart_dcache_wb_inval(a,l)
#define __dcache_wb(a, l)		heart_dcache_wb_inval(a,l)
#else
#define	__dcache_wb_inval(a,b)
#define	__dcache_wb_wb(a,b)
#endif	/* HEART_COHERENCY_WAR */

#if DEBUG
#define DPRINTF(x)		printf x
#else
#define DPRINTF(x)
#endif


void
prad_reset(void)
{

    RAD_WRITE(pbase,RAD_RESET,0xfffffffe);

}

void
prad_mute_relay(void)
{

    DPRINTF(("mute relay\n"));
    RAD_WRITE(pbase, RAD_GPIO1, 2);

}

void
prad_unmute_relay(void)
{

    DPRINTF(("unmute relay\n"));
    RAD_WRITE(pbase, RAD_GPIO1, 3);

}

void
prad_mute_dac(void)
{

    DPRINTF(("mute dac\n"));
    RAD_WRITE(pbase, RAD_GPIO2, 3);

}

void
prad_unmute_dac(void)
{

    DPRINTF(("unmute dac\n"));
    RAD_WRITE(pbase, RAD_ATOD_CONTROL, 0x03000000);	/* this is for P1 IP30 */
    RAD_WRITE(pbase, RAD_GPIO2, 2);

}

int
prad_probe(void)
{

    /* defined in libsk/ml/bridge.c */
    extern unsigned	pci_get_vendpart_nofault(bridge_t *bridge,
				volatile unsigned char *cfg);
    unsigned int	rad_id;
    static int		audio_hardware;		/* 0 = audio hardware present
						  -1 = no audio hardware */
    static int		probed;
    /*REFERENCED*/

    if (probed)
	return audio_hardware;

    pcfgbase = (rad_reg_t*)PHYS_TO_K1(RAD_PCI_CFG_BASE);
    pbase = (rad_reg_t*) PHYS_TO_K1(RAD_PCI_DEVIO_BASE);

    DPRINTF(("prad_probe: pcfgbase 0x%x, pbase 0x%x\n", pcfgbase, pbase));
    DPRINTF(("  RAD_PCI_CFG_BASE 0x%x, RAD_PCI_DEVIO_BASE 0x%x\n",
    	RAD_PCI_CFG_BASE, RAD_PCI_DEVIO_BASE));

    rad_id =  pci_get_vendpart_nofault(BRIDGE_K1PTR,
	(volatile unsigned char *)(pcfgbase + RAD_CFG_ID));
    probed++;

    if ( rad_id != 0x510a9 ) {
	DPRINTF(("rad_id (%x) != 0x510a9\n", rad_id));
	audio_hardware = -1;
	return audio_hardware;		/* RAD not found */
    }

    DPRINTF(("prad_probe: rad(id=0x%x)\n", rad_id));

    return(audio_hardware);

}

int
prad_setup(int volume)
{

    unsigned int	uitmp;

    /* setup tmp bridge config for rad
     * this 8is before init_ip30_chips
     *

	1. set up the bridge device register (0x900000001f00021c), 
		use PCI_MAPPED_MEM_BASE(BRIDGE_RAD_ID) >> 20
		for the DEV_OFF field, 
		and set the appropriate device attributes
	2. set up the base address registers (0x900000001f023010 and
		0x900000001f023014) in the PCI configuration space using
		PCI_MAPPED_MEM_BASE(BRIDGE_RAD_ID) again
	3. make sure the hello tune is done before calling init_ip30_chips()
     *
     * XXX this is done yet
     */

    /*
     * set the RAD's memory base

    RAD_CFG_WRITE(pcfgbase,RAD_CFG_MEMORY_BASE,PCI_MAPPED_MEM_BASE(BRIDGE_RAD_ID));
     */
    DPRINTF(("RAD_CFG_MEMORY_BASE 0x%x\n",
    	RAD_CFG_READ(pcfgbase, RAD_CFG_MEMORY_BASE) )); 
    DPRINTF(("PCI_MAPPED_MEM_BASE(BRIDGE_RAD_Id) 0x%x\n",
	PCI_MAPPED_MEM_BASE(BRIDGE_RAD_ID) ));

    /*
     * do some PCI config space setup
     */

    RAD_CFG_WRITE(pcfgbase,RAD_CFG_STATUS,6);
    RAD_CFG_WRITE(pcfgbase,RAD_CFG_LATENCY,~0);	/* something large to the latency reg */

    /*
     * do some stuff for RAD to become a PCI master
     */

    uitmp = (64 << 21) +        /* bits 31:21 => retry timer value = 64 */
	(16 << 0);          /* bits 7:0 -> holdoff value = 16 */
    RAD_WRITE(pbase,RAD_PCI_HOLDOFF,uitmp);

    DPRINTF(("prad_setup: RAD_PCI_HOLDOFF pbase 0x%x\n", pbase));
    RAD_WRITE(pbase,RAD_PCI_ARB_CONTROL,0xfac688);	/* default priority */
    DPRINTF(("prad_setup: RAD_PCI_ARB_CONTROL\n"));

    /*
     * take care of the reset and misc control registers
     */

    RAD_WRITE(pbase,RAD_RESET,0xfffffffe);	/* leave everything in reset */
    RAD_WRITE(pbase,RAD_MISC_CONTROL,0x480);	/* default state (except for VOtMUTE) */

    /*
     * set volume in and out
     */

    uitmp = 0xc0c0c0c0;
    SETBITS(uitmp,0xFF,volume,24); /* left output volume */
    SETBITS(uitmp,0xFF,volume,16); /* right output volume */
    RAD_WRITE(pbase,RAD_VOLUME_CONTROL,uitmp);

    DPRINTF(("prad_setup: done\n"));

    return(0);

}

int
prad_setup_dtoa( __psunsigned_t bufptr, unsigned int bufsize )
{

    unsigned int	uitmp;
    unsigned int	linecnt;
    __psunsigned_t	bufpci;

    /*
     * setup DtoA conv. clock
     */

    RAD_WRITE(pbase, RAD_CLOCKGEN_ICTL, 0x02000001);

    /*
     * setup FIRM #2 for 48K 
     */

    RAD_WRITE(pbase, RAD_FREQ_SYNTH2_MUX_SEL, 0x1);
    RAD_WRITE(pbase, RAD_CLOCKGEN_REM, 0x0000ffff);
#ifdef COMMENT
    RAD_WRITE(pbase, RAD_CLOCKGEN_ICTL, 0x40000403);	/* 48KHz */
#endif
    RAD_WRITE(pbase, RAD_CLOCKGEN_ICTL, 0x40000405);	/* 32KHz */

    /*
     * take FIRM #2 out of reset
     */

    RAD_WRITE(pbase, RAD_RESET, 0xffffcbfe);

    /*
     * setup DtoA to use FIRM #2
     */

    RAD_WRITE(pbase, RAD_DTOA_CONTROL, 0x40000000);

    /*
     * take internal DtoA interface out of reset followed by external DtoA interface
     */

    RAD_WRITE(pbase, RAD_RESET, 0xffffcbfc);	/* internal DtoA, bit 2 */

    us_delay(1000);

    RAD_WRITE(pbase, RAD_RESET, 0xffff8bfc);  /* external DtoA, bit 14 */

    /*
     * unmute the DAC
     */

    prad_unmute_dac();

    us_delay(10000);	/* may not be enough, spec calls for 100 msec */

    /*
     * setup working registers for one-shot DtoA DMA
     */

    bufpci = kv_to_bridge32_dirmap((void *)BRIDGE_K1PTR, bufptr);
    uitmp = (unsigned int)(bufpci & 0xffffffff);
    RAD_WRITE(pbase, RAD_PCI_LOADR_DTOA, uitmp);
    DPRINTF(("RAD_PCI_LOADR_DTOA %x uitmp, RAD_READ %x\n",
    	uitmp, RAD_READ(pbase, RAD_PCI_LOADR_DTOA)));

    uitmp = (unsigned int)(bufpci >> 32) & 0xffffffff;
    RAD_WRITE(pbase, RAD_PCI_HIADR_DTOA, uitmp);
    DPRINTF(("RAD_PCI_HIADR_DTOA %x uitmp\n",
    	uitmp, RAD_READ(pbase, RAD_PCI_HIADR_DTOA)));

    linecnt = bufsize / RAD_CACHE_LINE_WORD_SIZE;

    uitmp = 0;
    SETBITS(uitmp,0x1FFFFFF,-linecnt,7); /* bits 31:7 -> -line cnt */
    SETBITS(uitmp,0xF,0,3);        /* bits 6:3 -> next descriptor ptr */
    SETBITS(uitmp,0x1,1,2);        /* bit 2 -> end of descriptor, 1 = true */
    RAD_WRITE(pbase, RAD_PCI_CONTROL_DTOA, uitmp);

    /*
     * unmute the relay now
     */

    prad_unmute_relay();

    return(0);

}

void
prad_start_dtoa_dma(void)
{
    /*
     * start one-shot DtoA DMA
     */

    DPRINTF(("start dma\n"));
    RAD_WRITE(pbase, RAD_DTOA_CONTROL, 0x40000001);
}

int
prad_drain_dtoa_dma(int ms_wait)
{
    while ( (RAD_READ(pbase, RAD_DTOA_CONTROL) & 0x00000001) == 1 ) {
	if (--ms_wait <= 0) {
	    return(-1);
	}
    }
    return(0);
}

#endif /* !NOGFX && !NOBOOTTUNE */
