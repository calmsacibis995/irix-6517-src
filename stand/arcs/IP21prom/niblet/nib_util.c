#include "../pod.h"

int save_and_call(int [], int (*)(), unsigned int);

extern int num_tests;

void podHandler()
{
	pod_puts("Can't call podHandler from Niblet.\r\n");
	while (0);
}


int call_asm(int (*function)(unsigned int), unsigned int parm)
{
	int sregs[40];	/* 9 sregs, ra * 2 ints + one spare for alignment */
	int result;

	result = save_and_call(sregs, function, parm);

	return result;
}

int lo_ishex(int c)
{
        if ((c >= '0' && c <= '9') ||
            (c >= 'A' && c <= 'F') ||
            (c >= 'a' && c <= 'f')) {
                return 1;
        }
        else
                return 0;
}


void run_vector()
{
	int letter;
	int num;
	int quit = 0;

	while (!quit) {
		loprintf("Run which set of Niblet tests? (0-%b, q)\n",
							num_tests - 1);
		letter = pod_getc();
		if (letter == 'q')
			quit = 1;
		else if (!lo_ishex(letter))
			loprintf("\n*** Must be a hex digit\n");
		else {

			if (letter <= '9')
				num = letter - '0';
			else if (letter >= 'a')
				num = letter - 'a';
			else
				num = letter - 'A';
			loprintf("\nRunning test set 0x%b\n", num);
			do_niblet(num);
		}
	}
}

