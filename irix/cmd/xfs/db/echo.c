#ident "$Revision: 1.5 $"

#include <stdio.h>
#include "command.h"
#include "echo.h"
#include "output.h"

static int	echo_f(int argc, char **argv);

static const cmdinfo_t	echo_cmd =
	{ "echo", NULL, echo_f, 0, -1, 0, "[args]...",
	  "echo arguments", NULL };

/*ARGSUSED*/
static int
echo_f(
	int	argc,
	char	**argv)
{
	char	*c;

	for (c = *argv; c; c = *++argv)
		dbprintf("%s ", c);
	dbprintf("\n");
	return 0;
}

void
echo_init(void)
{
	add_command(&echo_cmd);
}
