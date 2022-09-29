#ident	"IP12diags/dsp/dsp_sram.c:  $Revision: 1.4 $"

/* 
 * dsp_sram.c - test dsp static ram
 */

#include "uif.h"
#include "sys/cpu.h"
#include "sys/sbd.h"

#define MEMBASE		PHYS_TO_K1(HPC1MEMORY)
#define MEMCOUNT	(32*1024)		/* 32 K of memory */
#define MEMSIZE		4			/* access in bytes */
#define MEMMASK		0x00ffffff		/* only 24 bits */

int membinarybit (struct range *, unsigned int);

int
dsp_sram ()
{
    int errcount = 0;
    struct range ra;

    if(is_v50()) {
	return errcount;
    }
    msg_printf (VRB, "DSP static RAM test\n");

    *(volatile unsigned int *)PHYS_TO_K1(HPC1MISCSR) = HPC1MISC_RESET;
    ra.ra_base = MEMBASE;
    ra.ra_count = MEMCOUNT;
    ra.ra_size = MEMSIZE;
    errcount = membinarybit (&ra, MEMMASK);

    if (errcount)
	sum_error ("DSP static RAM");

    return errcount;
}
