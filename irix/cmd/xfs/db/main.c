#ident "$Revision: 1.6 $"

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/fs/xfs_types.h>
#include <sys/fs/xfs_inum.h>
#include "sim.h"
#include "command.h"
#include "data.h"
#include "init.h"
#include "input.h"

main(
	int	argc,
	char	**argv)
{
	int	c;
	int	done;
	char	*input;
	char	**v;

	pushfile(stdin);
	init(argc, argv);
	done = 0;
	while (!done) {
		if ((input = getline()) == NULL)
			break;
		v = breakline(input, &c);
		if (c)
			done = command(c, v);
		doneline(input, v);
	}
	return exitcode;
}
