#ident "$Revision: 1.12 $"

#include <stdio.h>
#include "command.h"
#include "quit.h"

static int	quit_f(int argc, char **argv);

static const cmdinfo_t	quit_cmd =
	{ "quit", "q", quit_f, 0, 0, 0, NULL,
	  "exit xfs_db", NULL };

/*ARGSUSED*/
static int
quit_f(
	int	argc,
	char	**argv)
{
	return 1;
}

void
quit_init(void)
{
	add_command(&quit_cmd);
}
