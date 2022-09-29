/* binarybit.c - test ram address/register against data pattern */

#ident	"IP22diags/mem/binarybit.c:  $Revision: 1.11 $"

#include <sys/types.h>
#include <sys/sbd.h>
#include <sys/cpu.h>
#include <fault.h>
#include "setjmp.h"
#include "libsk.h"
#include "libsc.h"
#include "mem.h"

#undef BUSY_COUNT
#if TFP
#define	BUSY_COUNT		0x200
#else
#define	BUSY_COUNT		0x8000
#endif

#define	NUMBER_OF_PATTERNS	(sizeof(binary_pattern)/sizeof(__psunsigned_t))

#define	BINARY_BIT(TYPE,COUNT)						\
{									\
	register volatile TYPE	*ptr = (TYPE *)ra->ra_base;		\
									\
	while (--count >= 0) {						\
		for (i = 0; i < COUNT; i++) {				\
			*ptr = (TYPE)masked_pattern[i];			\
			msg_printf(DBG,"Testing %x with %x\n",ptr, *ptr);	\
			*cpuctrl0;					\
			if (*ptr != masked_pattern[i]) {		\
				error_count++;				\
				memerr((caddr_t)ptr,			\
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
membinarybit(struct range *ra, __psunsigned_t mask)
{

#if _MIPS_SIM == _ABI64
	static	__psunsigned_t	binary_pattern[] = {
					0x0,
					0xaaaaaaaaaaaaaaaa,
					0xcccccccccccccccc,
					0xf0f0f0f0f0f0f0f0,
					0xff00ff00ff00ff00,
					0xffff0000ffff0000,
				};


#else
	static u_int		binary_pattern[] = {
					0x00000000,
					0xaaaaaaaa,
					0xcccccccc,
					0xf0f0f0f0,
					0xff00ff00,
					0xffff0000,
				};

#endif
	register int		busy_count = 0;
	register __psint_t	count = ra->ra_count;
	register volatile u_int	*cpuctrl0 = (u_int *)PHYS_TO_K1(CPUCTRL0);
	int			error_count = 0;
	jmp_buf 		faultbuf;
	register int 		i;
	volatile __psunsigned_t		masked_pattern[NUMBER_OF_PATTERNS * 2];

#if IP26
	flush_cache();
#endif

	/* zero out unused bits */
	for (i = 0; i < NUMBER_OF_PATTERNS; i++) {
		msg_printf(DBG,"binarypattern[%d] = %016x ~= %016x\n",i,binary_pattern[i], ~binary_pattern[i]);
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

#if _MIPS_SIM == _ABI64
	case sizeof(__psunsigned_t):
		BINARY_BIT(__psunsigned_t, 12);
#endif
	}

	busy(0);
	nofault = 0;			/* disarm the interrupt handler */

	return error_count;
}
