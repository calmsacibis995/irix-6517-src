#ident	"IP22diags/mem/parity.c:  $Revision: 1.21 $"

/*
 * parity.c - IDE parity checking mechanism test
 *
 * Memparity is a driver that calls byte, halfword,
 * and word parity tests with suitable parameters.
 */
#include "private.h"
#include "sys/types.h"
#include "sys/cpu.h"
#include "sys/sbd.h"
#include "libsk.h"
#include "libsc.h"
#include "uif.h"
#include "mem.h"

#define parbyte(a,v) parcheck(a,v,sizeof(char))
#define parhalf(a,v) parcheck(a,v,sizeof(short))
#define parword(a,v) parcheck(a,v,sizeof(int))
/* this ensures we can access the other half of the double word */
__psunsigned_t default_location[3];
extern int gr2_dmafill();

bool_t
parity_test(int argc, char **argv)
{
	struct range range;
	register bool_t ok;
	register bool_t ok1;

	msg_printf(VRB, "Parity mechanism test\n");

#if IP26 || IP28
	if (ip26_isecc()) return 0;		/* skip test on new board */
	flush_cache();
#endif

	/* default test parameters*/
	range.ra_base = (__psint_t)PHYS_TO_K1(KDM_TO_PHYS((__psunsigned_t)&default_location[1] & ~0x7));
	range.ra_count = 2;
	range.ra_size = sizeof(__int32_t);

	if ((argc == 2 && !parse_range(argv[1], sizeof(unsigned int), &range))
	    || argc > 2) {
		msg_printf(ERR, "Usage: %s [RANGE]\n", argv[0]);
		return 1;
	}

	if (!IS_KSEG1(range.ra_base))
		range.ra_base = (__psint_t)PHYS_TO_K1(KDM_TO_PHYS(range.ra_base));

	ok = memparity(&range, BIT_TRUE, RUN_UNTIL_ERROR, 0);
	if ( !ok ) {
		msg_printf(DBG,"memparity FAILED w/ BIT_TRUE\n");
	} else {
		msg_printf(DBG,"memparity PASSED w/ BIT_TRUE\n");
	}
	ok1 = memparity(&range, BIT_INVERT, RUN_UNTIL_ERROR, 0);
	if ( !ok1 ) {
		msg_printf(DBG,"memparity FAILED w/ BIT_INVERT\n");
	} else {
		msg_printf(DBG,"memparity PASSED w/ BIT_INVERT\n");
	}
	ok = ok && ok1;

	/*
	 * there's a bug in the DMUX such that when it generates a bad parity,
	 * the GIO side of it gets confused and causes subsequent DMA to fail.
	 * we get around this by flushing the DMUX FIFO after we are done with
	 * the parity test
	 */
#ifdef IP20
	{
		extern unsigned int _gio64_arb;

		*(volatile unsigned int *)PHYS_TO_K1(GIO64_ARB) =
			_gio64_arb | GIO64_ARB_EXP1_PIPED;
		(void)wbadaddr((void *)PHYS_TO_K1(0x1f9ffffc), 4);
		*(volatile unsigned int *)PHYS_TO_K1(GIO64_ARB) = _gio64_arb;
	}
#else
	DMUXfix ();
#endif

	if (ok) 
		okydoky();
	else
		sum_error("parity mechanism");

	return ok ? 0 : 1;
}

/*ARGSUSED3*/
bool_t
memparity(struct range *ra, enum bitsense sense, enum runflag till,
	  void (*errfun)())
{
	register char *addr = (char *)ra->ra_base;
	register __psint_t count = ra->ra_count;
	register bool_t parok = TRUE;

	msg_printf(DBG,"memparity: looping 0x%x times\n", count);
	while (--count >= 0) {
		register unsigned int val;

		if (ra->ra_size >= sizeof(unsigned char)) {
			val = (sense == BIT_TRUE) ? 0x55 : 0xce;
			parok &= parbyte((unsigned char *)addr, val);
			if ( !parok ) {
				msg_printf(DBG,"byte 0 failed,");
			}
			parok &= parbyte((unsigned char *)addr + 1, val);
			if ( !parok ) {
				msg_printf(DBG,"byte 1 failed,");
			}
			parok &= parbyte((unsigned char *)addr + 2, val);
			if ( !parok ) {
				msg_printf(DBG,"byte 2 failed,");
			}
			parok &= parbyte((unsigned char *)addr + 3, val);
			if ( !parok ) {
				msg_printf(DBG,"byte 3 failed,");
			}
			parok &= parbyte((unsigned char *)addr + 4, val);
			if ( !parok ) {
				msg_printf(DBG,"byte 4 failed,");
			}
			parok &= parbyte((unsigned char *)addr + 5, val);
			if ( !parok ) {
				msg_printf(DBG,"byte 5 failed,");
			}
			parok &= parbyte((unsigned char *)addr + 6, val);
			if ( !parok ) {
				msg_printf(DBG,"byte 6 failed,");
			}
			parok &= parbyte((unsigned char *)addr + 7, val);
			if ( !parok ) {
				msg_printf(DBG,"byte 7 failed,");
			}
		}

		if (ra->ra_size >= sizeof(unsigned short)) {
			val = (sense == BIT_TRUE) ? 0x5555 : 0x3332;
			parok &= parhalf((unsigned short *)addr, val);
			if ( !parok ) {
				msg_printf(DBG,"short 0 failed,");
			}
			parok &= parhalf((unsigned short *)addr + 1, val);
			if ( !parok ) {
				msg_printf(DBG,"short 1 failed,");
			}
			parok &= parhalf((unsigned short *)addr + 2, val);
			if ( !parok ) {
				msg_printf(DBG,"short 2 failed,");
			}
			parok &= parhalf((unsigned short *)addr + 3, val);
			if ( !parok ) {
				msg_printf(DBG,"short 3 failed,");
			}
		}

		if (ra->ra_size >= sizeof(unsigned int)) {
			val = (sense == BIT_TRUE) ? 0x55555555 : 0xaaaaaaab;
			parok &= parword((unsigned int *)addr, val);
			if ( !parok ) {
				msg_printf(DBG,"long 0 failed,");
			}
			parok &= parword((unsigned int *)addr + 1, val);
			if ( !parok ) {
				msg_printf(DBG,"long 1 failed,");
			}
			parok &= pardisenw((unsigned int *)addr);
			if ( !parok ) {
				msg_printf(DBG,"pardisenw: FAILED,");
			}
		}

		if (!parok) {
			if (till == RUN_UNTIL_ERROR)
				break;
		}

		addr += ra->ra_size;
	}
	return parok;
}
