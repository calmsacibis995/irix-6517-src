#ident	"IP22diags/mem/walkingbit.c:  $Revision: 1.2 $"

/*
 * walkingbit.c - IDE walking bit data path test
 *
 * The data bus is tested by doing a "walking 1" or "walking 0" test over
 * a given range of memory, loading and storing in byte, half-word, or word
 * units as specified by ra->ra_size.
 *
 * Checked and tested for IP32
 * contains IP22 specific code, see preproc. directives
 * 6/2/96
 *
 */

#include <fault.h>
#include "setjmp.h"
#include "mem.h"
#include <sys/cpu.h>
#include <sys/sbd.h>
#include <sys/types.h>

#ifdef  PiE_EMULATOR
#define BUSY_COUNT      0x80
#else
#define BUSY_COUNT      0x4000
#endif  /* PiE_EMULATOR */

#ifndef IP32
#define	WALKING_ONE(TYPE)						\
{									\
	register volatile u_int	*cpuctrl0 =				\
		(u_int *)PHYS_TO_K1(CPUCTRL0);				\
	register volatile TYPE	*ptr;					\
	register TYPE		walk_mask;				\
									\
	ptr = (TYPE *)ra->ra_base;					\
	walk_mask = sense == BIT_INVERT ? ~0x0 : 0x0;			\
	while (--count >= 0) {						\
		register TYPE	walking_1s = 0x1;			\
									\
		do {							\
			register TYPE	value;				\
									\
			value = walking_1s ^ walk_mask;			\
			*ptr = value;					\
			*cpuctrl0;					\
			if (*ptr != value) {				\
				passflag = FALSE;			\
				if (errfun) 				\
					(*errfun)((u_int)ptr, value,	\
						*ptr, sizeof(TYPE));	\
				if (till == RUN_UNTIL_ERROR)		\
					return passflag;		\
			}						\
			walking_1s <<= 1;				\
		} while (walking_1s);					\
									\
		ptr++;							\
		if (!busy_count--) {					\
			busy_count = BUSY_COUNT;			\
			busy(1);					\
		}							\
	}								\
	break;								\
}
#else /* !IP32 */
#define	WALKING_ONE(TYPE)						\
{									\
	register volatile TYPE	*ptr;					\
	register TYPE		walk_mask;				\
									\
	ptr = (TYPE *)ra->ra_base;					\
	walk_mask = sense == BIT_INVERT ? ~0x0 : 0x0;			\
	while (--count >= 0) {						\
		register TYPE	walking_1s = 0x1;			\
									\
		do {							\
			register TYPE	value;				\
									\
			value = walking_1s ^ walk_mask;			\
			*ptr = value;					\
			if (*ptr != value) {				\
				passflag = FALSE;			\
				if (errfun) 				\
					(*errfun)((u_int)ptr, value,	\
						*ptr, sizeof(TYPE));	\
				if (till == RUN_UNTIL_ERROR)		\
					return passflag;		\
			}						\
			walking_1s <<= 1;				\
		} while (walking_1s);					\
									\
		ptr++;							\
		if (!busy_count--) {					\
			busy_count = BUSY_COUNT;			\
			busy(1);					\
		}							\
	}								\
	break;								\
}
#endif /* !IP32 */

bool_t
memwalkingbit(struct range *ra, enum bitsense sense, enum runflag till,
	void (*errfun)())
{
	register int	busy_count = 0;
	register int	count = ra->ra_count;
	jmp_buf		faultbuf;
	bool_t		passflag = TRUE;

	msg_printf(VRB, "CPU local memory walking bit test\n");

	if (setjmp(faultbuf)) {
		busy(0);
		show_fault();
		return FALSE;
	}
	nofault = faultbuf;

	busy(0);
	switch (ra->ra_size) {
	case sizeof(u_int):
		WALKING_ONE(u_int)

	case sizeof(u_short):
		WALKING_ONE(u_short)

	case sizeof(u_char):
		WALKING_ONE(u_char)
	}

	busy(0);
	nofault = 0;
	return passflag;
}
