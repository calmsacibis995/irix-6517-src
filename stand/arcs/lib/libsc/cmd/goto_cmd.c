#ident "$Revision: 1.3 $"

/*
 * goto -- Jump to a specific address in memory
 *	Up to four arguments may be passed, which will then
 *	be converted into numbers and stuffed into a0-a3 before
 *	the routine is called.
 */

#include <arcs/io.h>
#include <libsc.h>

int
goto_cmd(int argc, char **argv)
{
	unsigned arg[4];	/* Arguments passed to function */
				/* Function to be called */
	unsigned (*func)(unsigned, unsigned, unsigned, unsigned);
	unsigned long long value;
	unsigned retval;
	int i;

	if (argc < 2)
		return 1;

        /* Parse off the address */
	if (*atobu_L(argv[1], &value)) {
		printf("Invalid address: %s\n", argv[1]);
		return 1;
	}

	func = (unsigned (*)()) value;

	/* Parse off the arguments */
	for (i = 2; i < argc; i++) {
		if (*atobu(argv[i], &arg[i-2])) {
			printf("Argument %s is not a number: %s\n", argv[i]);
			return 1;
		}
	}

	/* Call function and display return value, if any */
	printf("Calling function at 0x%x\n", (__scunsigned_t) func);
	retval = func(arg[0], arg[1], arg[2], arg[3]); 

	printf("Function returned a value of 0x%x\n", retval);

	return 0;
}
