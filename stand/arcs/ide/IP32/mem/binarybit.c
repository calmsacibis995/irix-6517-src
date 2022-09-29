/* binarybit.c - test ram address/register against data pattern */

#ident	"IP22diags/mem/binarybit.c:  $Revision: 1.1 $"

#include <fault.h>
#include "setjmp.h"
#include "mem.h"
#include <sys/cpu.h>
#include <sys/sbd.h>
#include <sys/types.h>

#define	BUSY_COUNT		0x8000
#define	NUMBER_OF_PATTERNS	(sizeof(binary_pattern) / sizeof(u_int))

#define	BINARY_BIT(TYPE,COUNT)						\
{									\
	register volatile TYPE	*ptr = (TYPE *)ra->ra_base;		\
									\
	while (--count >= 0) {						\
		for (i = 0; i < COUNT; i++) {				\
			*ptr = masked_pattern[i];			\
			*cpuctrl0;					\
			if (*ptr != masked_pattern[i]) {		\
				error_count++;				\
				memerr((u_int)ptr,			\
					masked_pattern[i], 		\
					*ptr, sizeof(TYPE));		\
			}						\
									\
			if (!busy_count--) {				\
				busy_count = BUSY_COUNT;		\
				busy(1);				\
			}						\
		}							\
		ptr++;							\
	}								\
	break;								\
}

bool_t
membinarybit(struct range *ra, u_int mask)
{
	static u_int		binary_pattern[] = {
					0x00000000,
					0xaaaaaaaa,
					0xcccccccc,
					0xf0f0f0f0,
					0xff00ff00,
					0xffff0000,
				};
	register int		busy_count = 0;
	register int		count = ra->ra_count;
#ifndef IP32
	register volatile u_int	*cpuctrl0 = (u_int *)PHYS_TO_K1(CPUCTRL0);
#endif /* !IP32 */
	int			error_count = 0;
	jmp_buf 		faultbuf;
	register int 		i;
	u_int 			masked_pattern[NUMBER_OF_PATTERNS * 2];

	/* zero out unused bits */
	for (i = 0; i < NUMBER_OF_PATTERNS; i++) {
		masked_pattern[i * 2] = binary_pattern[i] & mask;
		masked_pattern[i * 2 + 1] = ~binary_pattern[i] & mask;
	}

	/* set up exception handler */
	if (setjmp(faultbuf)) {
		show_fault();
		return FALSE;
	}
	nofault = faultbuf;

	busy(0);
	switch (ra->ra_size) {
	case sizeof(u_char):
		BINARY_BIT(u_char, 8);

	case sizeof(u_short):
		BINARY_BIT(u_short, 10);

	case sizeof(u_int):
		BINARY_BIT(u_int, 12);
	}

	busy(0);
	nofault = 0;			/* disarm the interrupt handler */

	return error_count;
}
