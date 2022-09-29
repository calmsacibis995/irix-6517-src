#ident	"IP22diags/mem/addruniq.c:  $Revision: 1.1 $"

/*
 * addruniq.c - IDE address uniqueness test
 *
 *
 * An address uniqueness test is done over a specified range of memory by
 * writing addresses to themselves and reading them back.
 */

#include <fault.h>
#include "mem.h"
#include "setjmp.h"
#include <sys/types.h>

#ifdef	PiE_EMULATOR
#define	BUSY_COUNT	0x8000
#else
#define	BUSY_COUNT	0x200000
#endif	/* PiE_EMULATOR */

bool_t
memaddruniq(struct range *ra, enum bitsense sense, enum runflag till,
	void (*errfun)())
{
	register int		busy_count = 0;
	register int		count;
	jmp_buf			faultbuf;
	bool_t			passflag = TRUE;
	register volatile u_int	*ptr;

	msg_printf(VRB, "CPU local memory address uniqueness test\n");

	if (setjmp(faultbuf)) {
		busy(0);
		show_fault();
		return FALSE;
	}
	nofault = faultbuf;

	busy(0);
	count = ra->ra_count;
	/* use 2 loops such that we don't have to test sense within loop */
	if (sense == BIT_TRUE) {
		for (ptr = (u_int *)ra->ra_base; --count >= 0; ptr++) {
			*ptr = (u_int)ptr;
			if (!busy_count--) {
				busy_count = BUSY_COUNT;
				busy(1);
			}
		}
	} else {
		for (ptr = (u_int *)ra->ra_base; --count >= 0; ptr++) {
			*ptr = ~(u_int)ptr;
			if (!busy_count--) {
				busy_count = BUSY_COUNT;
				busy(1);
			}
		}
	}

	count = ra->ra_count;
	/* use 2 loops such that we don't have to test sense within loop */
	if (sense == BIT_TRUE) {
		for (ptr = (u_int *)ra->ra_base; --count >= 0; ptr++) {
			if (*ptr != (u_int)ptr) {
				passflag = FALSE;
				if (errfun)
					(*errfun)((u_int)ptr, (u_int)ptr,
						*ptr, sizeof(u_int));
				if (till == RUN_UNTIL_ERROR)
					break;
			}

			if (!busy_count--) {
				busy_count = BUSY_COUNT;
				busy(1);
			}
		}
	} else {
		for (ptr = (u_int *)ra->ra_base; --count >= 0; ptr++) {
			if (*ptr != ~(u_int)ptr) {
				passflag = FALSE;
				if (errfun)
					(*errfun)((u_int)ptr, ~(u_int)ptr,
						*ptr, sizeof(u_int));
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
