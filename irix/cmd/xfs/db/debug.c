#ident "$Revision: 1.3 $"

#include <stdio.h>
#include <stdlib.h>
#include "command.h"
#include "debug.h"
#include "output.h"

static int	debug_f(int argc, char **argv);

static const cmdinfo_t	debug_cmd =
	{ "debug", NULL, debug_f, 0, 1, 0, "[flagbits]",
	  "set debug option bits", NULL };

long	debug_state;

static int
debug_f(
	int	argc,
	char	**argv)
{
	char	*p;

	if (argc == 1) {
		debug_state = strtol(argv[0], &p, 0);
		if (*p != '\0') {
			dbprintf("bad value for debug %s\n", argv[0]);
			return 0;
		}
	}
	dbprintf("debug = %ld\n", debug_state);
	return 0;
}

void
debug_init(void)
{
	add_command(&debug_cmd);
}
