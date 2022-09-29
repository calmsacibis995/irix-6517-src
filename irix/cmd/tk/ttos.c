#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "tkm.h"
#include "tk_private.h"

int
main(int argc, char **argv)
{
	int i;

	for (i = 1; i < argc; i++) {
		tk_print_tk_set((unsigned int)strtol(argv[i], 0, 0), stdout, 0);
	}
	return 0;
}

/* ARGSUSED */
tks_ch_t getch_t(void *a){return 0;}
