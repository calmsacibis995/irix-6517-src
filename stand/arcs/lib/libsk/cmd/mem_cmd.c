/*
 * general memory modification commands
 */

#ident	"$Revision: 1.22 $"

#include <libsc.h>
#include <libsk.h>

extern void showchar(int);
extern void mem_error(char *);
extern unsigned long long get_memory(__psunsigned_t, int);

/*
 * get -- read memory
 */
int
get(int argc, char **argv)
{
	int width = SW_WORD;
	unsigned long long val;
	unsigned long long address;

	argv++; argc--;
	if (argc == 2 && **argv == '-') {
		switch ((*argv)[1]) {
		case 'b':
			width = SW_BYTE;
			break;

		case 'h':
			width = SW_HALFWORD;
			break;

		case 'w':
			width = SW_WORD;
			break;

		case 'd':
			width = SW_DOUBLEWORD;
			break;

		default:
			return(1);
		}
		argv++; argc--;
	}

	if (argc != 1)
		return(1);

	if (*atobu_L(*argv, &address))
		return(1);

	val = get_memory(address, width);
	printf("0x%x:\t%Ld\t0x%Lx\t'", address, val, val);
	showchar(val);
	printf("'\n");
	return(0);
}

/*
 * put -- write memory
 */
int
put(int argc, char **argv)
{
	int width = SW_WORD;
	unsigned long long address;
	unsigned long long val;

	argv++; argc--;
	if (argc == 3 && **argv == '-') {
		switch ((*argv)[1]) {
		case 'b':
			width = SW_BYTE;
			break;

		case 'h':
			width = SW_HALFWORD;
			break;

		case 'w':
			width = SW_WORD;
			break;

#if _MIPS_SIM != _ABIO32
		case 'd':
			width = SW_DOUBLEWORD;
			break;
#endif

		default:
			return(1);
		}
		argv++; argc--;
	}

	if (argc != 2)
		return(1);

	if (*atobu_L(argv[1], &val))
		return(1);

	if (*atobu_L(*argv, &address))
		return(1);

	set_memory((void *)address, width, (long)val);
	return(0);
}
