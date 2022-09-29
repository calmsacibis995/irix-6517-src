/*
 * memory routines used by mem_cmd.c, and dump_cmd.c.
 */

#ident "$Revision: 1.10 $"

#include <parser.h>
#include <libsc.h>
#include <libsk.h>

extern void mem_error(char *);

#define	SW_BYTE		1
#define	SW_HALFWORD	2
#define	SW_WORD		4

/*
 * set_memory -- set memory location
 */
void
set_memory(void *caddr, int width, long value)
{
	/* don't allow a simple "put" to overwrite protected memory */
	if (is_protected_adrs(
	    (unsigned long)caddr, (unsigned long)caddr + width))
		return;

	switch (width) {

	case SW_BYTE:
		*(unsigned char *)caddr = value;
		break;

	case SW_HALFWORD:
		*(unsigned short *)caddr = value;
		break;

	case SW_WORD:
		*(unsigned *)caddr = value;
		break;

	case SW_DOUBLEWORD:
		*(unsigned long *)caddr = value;
		break;
	
	default:
		mem_error("set_memory: Illegal switch");
	}
	clear_cache(caddr, width);
}

/*
 * fill -- fill block of memory with pattern
 */
int fill(int argc, char **argv)
{
	register __psunsigned_t base = 0;
	register __psint_t cnt = 0;
	register int size = SW_WORD;
/*	auto int value = 0; */
	auto __psint_t value = 0;

	if (argc < 2)
		return(1);

	while (--argc > 0) {
		if (**++argv == '-') {
			switch ((*argv)[1]) {
			case 'b':
				size = SW_BYTE;
				break;

			case 'h':
				size = SW_HALFWORD;
				break;

			case 'w':
				size = SW_WORD;
				break;

			case 'd':
				size = SW_DOUBLEWORD;
				break;

			case 'v':
				if (--argc < 1)
					return(1);

				if (*atob_ptr(*++argv, &value))
					return(1);
				break;

			default:
				return(1);
			}
		} else {
			struct range range;

			if (! parse_range(*argv, size, &range))
				return(1);

			base = range.ra_base;
			cnt = range.ra_count;
		}
	}


	switch (size) {
	case SW_BYTE:
		while (--cnt >= 0) {
			*(char *)base = value;
			base += size;
		}
		break;

	case SW_HALFWORD:
		while (--cnt >= 0) {
			*(short *)base = value;
			base += size;
		}
		break;

	case SW_WORD:
		while (--cnt >= 0) {
			*(int *)base = value;
			base += size;
		}
		break;

	case SW_DOUBLEWORD:

	while (--cnt >= 0) {
			*(__psunsigned_t *)base = (__psint_t)value;
                        base += size;
                }
                break;

	default:
		mem_error("fill");
	}
	return(0);
}
