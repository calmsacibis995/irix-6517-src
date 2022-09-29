#ident	"IP22diags/mem/addruniq.c:  $Revision: 1.12 $"

/*
 * addruniq.c - IDE address uniqueness test
 *
 *
 * An address uniqueness test is done over a specified range of memory by
 * writing addresses to themselves and reading them back.
 */

#include <sys/types.h>
#include <fault.h>
#include "mem.h"
#include "setjmp.h"
#include "libsk.h"
#include "libsc.h"

bool_t
memaddruniq(struct range *ra, enum bitsense sense, enum runflag till,
	void (*errfun)(char *, __psunsigned_t, __psunsigned_t, int))
{
	register int		busy_count = 0;
	register long		count;
	jmp_buf			faultbuf;
	bool_t			passflag = TRUE;
	register volatile __psunsigned_t *ptr;

	msg_printf(VRB, "CPU local memory address uniqueness test\n");

#if IP26
	flush_cache();
#endif

	if (setjmp(faultbuf)) {
		busy(0);
		show_fault();
		return FALSE;
	}
	nofault = faultbuf;

	busy(0);
	count = ra->ra_count;
#if _MIPS_SIM == _ABI64
	/* scale count to double words */
	if (ra->ra_size != sizeof(__psunsigned_t))
		count = (ra->ra_count*ra->ra_size)/(int)sizeof(__psunsigned_t);
#endif
	/* use 2 loops such that we don't have to test sense within loop */
	if (sense == BIT_TRUE) {
		for (ptr = (__psunsigned_t *)ra->ra_base; --count >= 0; ptr++) {
			*ptr = (__psunsigned_t)ptr;
			if (!busy_count--) {
				busy_count = BUSY_COUNT;
				busy(1);
			}
		}
	} else {
		for (ptr = (__psunsigned_t *)ra->ra_base; --count >= 0; ptr++) {
			*ptr = ~(__psunsigned_t)ptr;
			if (!busy_count--) {
				busy_count = BUSY_COUNT;
				busy(1);
			}
		}
	}

	count = ra->ra_count;
#if _MIPS_SIM == _ABI64
	/* scale count to double words */
	if (ra->ra_size != sizeof(__psunsigned_t))
		count = (ra->ra_count*ra->ra_size)/(int)sizeof(__psunsigned_t);
#endif
	/* use 2 loops such that we don't have to test sense within loop */
	if (sense == BIT_TRUE) {
		for (ptr = (__psunsigned_t *)ra->ra_base; --count >= 0; ptr++) {
			if (*ptr != (__psunsigned_t)ptr) {
				passflag = FALSE;
				if (errfun)
					(*errfun)((char *)ptr,
						(__psunsigned_t)ptr,
						*ptr, sizeof(ptr));
				if (till == RUN_UNTIL_ERROR)
					break;
			}

			if (!busy_count--) {
				busy_count = BUSY_COUNT;
				busy(1);
			}
		}
	} else {
		for (ptr = (__psunsigned_t *)ra->ra_base; --count >= 0; ptr++) {
			if (*ptr != ~(__psunsigned_t)ptr) {
				passflag = FALSE;
				if (errfun)
					(*errfun)((char *)ptr,
						(~(__psunsigned_t)ptr),
						*ptr, sizeof(ptr));
				if (till == RUN_UNTIL_ERROR)
					break;
			}

			if (!busy_count--) {
				busy_count = BUSY_COUNT;
				busy(1);
			}
		}
	}

	busy(0);
	nofault = 0;
	return passflag;
}
