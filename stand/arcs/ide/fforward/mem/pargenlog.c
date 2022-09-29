#ident	"IP22diags/mem/parity.c:  $Revision: 1.11 $"

/*
 * pargenlog.c - IDE parity generation logic test
 *
 */

#include <sys/sbd.h>
#include <sys/cpu.h>
#include <uif.h>
#include <libsk.h>
#include "private.h"
#include "mem.h"

static bool_t mempargen(struct range *, enum bitsense, enum runflag,
							void (*errfun)());

#define parbyte(a,v) parcheck(a,v,sizeof(char))
#define parhalf(a,v) parcheck(a,v,sizeof(short))
#define parword(a,v) parcheck(a,v,sizeof(int)) 
/* These are also defined in memtest.c */

/* this ensures we can access the other half of the double word */
int default_location2[3];
extern int gr2_dmafill();

bool_t
pargenlog_test(int argc, char **argv)
{
	struct range range;
	register bool_t ok;

	msg_printf(VRB, "Parity generation logic test\n");

#if IP26 || IP28
	if (ip26_isecc()) return 0;		/* skip test on new board */
	flush_cache();
#endif

	/* default test parameters*/
	range.ra_base = (__psint_t)PHYS_TO_K1(KDM_TO_PHYS((__psunsigned_t)&default_location2[1] & ~0x7));
/*	range.ra_count = 2;  */
	range.ra_size = sizeof(int);

	if ((argc == 2 && !parse_range(argv[1], sizeof(unsigned int), &range))
	    || argc > 2) {
		msg_printf(ERR, "Usage: %s [RANGE]\n", argv[0]);
		return 1;
	}

	if (!IS_KSEG1(range.ra_base))
		range.ra_base = (__psint_t)PHYS_TO_K1(KDM_TO_PHYS(range.ra_base));

	ok =  mempargen(&range, BIT_TRUE, RUN_UNTIL_ERROR, 0)
		&& mempargen(&range, BIT_INVERT, RUN_UNTIL_ERROR, 0);

	/*
	 * there's a bug in the DMUX such that when it generates a bad parity,
	 * the GIO side of it gets confused and causes subsequent DMA to fail.
	 * we get around this by flushing the DMUX FIFO after we are done with
	 * the parity test
	 */
	DMUXfix ();

	if (ok) 
		okydoky();
	else
		sum_error("parity generation mechanism");

	return ok ? 0 : 1;
}


/*****************************************************************************/
/*  How mempargen differs from memparity:
    ra.ra_count is not used
    no option provided for size of parcheck; using 32 bits
    Instead of using just one pattern, 2^8 patterns are used for one set of addresses. 
    The desired effect is to cycle throuh the different possible data patterns 
    and check for parity generation errors.  
    The original test covered whether we could 'force' badparity at every byte lane, for
    a range of addresses, but only 1 data pattern.
*/
/*****************************************************************************/


/*ARGSUSED4*/
static bool_t
mempargen(
	struct range *ra,
	enum bitsense sense,	/* XXX BIT_TRUE : even :: BIT_INVERT : odd */
	enum runflag till,
	void (*errfun)())
{
	register unsigned int *addr = (unsigned int *)ra->ra_base;
	register int count = 256;  /*  for the 2^8 combo's  */
	register bool_t parok = TRUE;
	register unsigned int val = (sense == BIT_TRUE) ? 0 : 0xffffffff;

	parok &= pardisenw(addr);  /*  taken out of the loop so that it would execute once only*/


	while (--count >= 0) {
msg_printf(DBG, "testing address: 0x%08x with data: 0x%x \n", addr, val);
		val = (sense == BIT_TRUE) ? (val + 0x01010101): (val - 0x01010101);
		parok &= parword(addr, val);
		parok &= parword(addr + 1, val);
		if (!parok) {
			if (till == RUN_UNTIL_ERROR)
				break;
		}
/*		addr = (char *)addr + ra->ra_size;*/
	}  /* end while*/
	return parok;
}











































