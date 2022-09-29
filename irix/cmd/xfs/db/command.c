#ident "$Revision: 1.18 $"

#include "versions.h"
#include <sys/param.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/fs/xfs_types.h>
#include <sys/fs/xfs_inum.h>
#include <sys/fs/xfs_alloc_btree.h>
#include "addr.h"
#include "agf.h"
#include "agfl.h"
#include "agi.h"
#include "block.h"
#include "bmap.h"
#include "check.h"
#include "command.h"
#include "convert.h"
#include "debug.h"
#include "type.h"
#include "echo.h"
#include "faddr.h"
#include "fprint.h"
#include "field.h"
#include "frag.h"
#include "freesp.h"
#include "help.h"
#include "hash.h"
#include "inode.h"
#include "input.h"
#include "io.h"
#include "output.h"
#include "print.h"
#include "quit.h"
#include "sb.h"
#include "write.h"
#include "malloc.h"
#if VERS >= V_62
#include "dquot.h"
#endif

cmdinfo_t	*cmdtab;
int		ncmds;

static int	cmd_compare(const void *a, const void *b);

static int
cmd_compare(const void *a, const void *b)
{
	return strcmp(((const cmdinfo_t *)a)->name,
		      ((const cmdinfo_t *)b)->name);
}

void
add_command(
	const cmdinfo_t	*ci)
{
	cmdtab = xrealloc((void *)cmdtab, ++ncmds * sizeof(*cmdtab));
	cmdtab[ncmds - 1] = *ci;
	qsort(cmdtab, ncmds, sizeof(*cmdtab), cmd_compare);
}

int
command(
	int		argc,
	char		**argv)
{
	char		*cmd;
	const cmdinfo_t	*ct;

	cmd = argv[0];
	ct = find_command(cmd);
	if (ct == NULL) {
		dbprintf("command %s not found\n", cmd);
		return 0;
	}
	argc--;
	argv++;
	if (argc < ct->argmin || (ct->argmax != -1 && argc > ct->argmax)) {
		dbprintf("bad argument count %d to %s, expected ", argc, cmd);
		if (ct->argmax == -1)
			dbprintf("at least %d", ct->argmin);
		else if (ct->argmin == ct->argmax)
			dbprintf("%d", ct->argmin);
		else
			dbprintf("between %d and %d", ct->argmin, ct->argmax);
		dbprintf(" arguments\n");
		return 0;
	}
	optind = 0;
	return ct->cfunc(argc, argv);
}

const cmdinfo_t *
find_command(
	const char	*cmd)
{
	cmdinfo_t	*ct;

	for (ct = cmdtab; ct < &cmdtab[ncmds]; ct++) {
		if (strcmp(ct->name, cmd) == 0 ||
		    (ct->altname && strcmp(ct->altname, cmd) == 0))
			return (const cmdinfo_t *)ct;
	}
	return NULL;
}

void
init_commands(void)
{
	addr_init();
	agf_init();
	agfl_init();
	agi_init();
	block_init();
	bmap_init();
	check_init();
	convert_init();
	debug_init();
	echo_init();
	frag_init();
	freesp_init();
	help_init();
	hash_init();
	inode_init();
	input_init();
	io_init();
	output_init();
	print_init();
	quit_init();
	sb_init();
	type_init();
	write_init();
#if VERS >= V_62
	dquot_init();
#endif
}
