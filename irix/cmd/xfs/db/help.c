#ident "$Revision: 1.3 $"

#include <stdio.h>
#include "command.h"
#include "help.h"
#include "output.h"

static void	help_all(void);
static void	help_onecmd(const char *cmd, const cmdinfo_t *ct);
static int	help_f(int argc, char **argv);
static void	help_oneline(const char *cmd, const cmdinfo_t *ct);

static const cmdinfo_t	help_cmd =
	{ "help", NULL, help_f, 0, 1, 0, "[command]",
	  "help for one or all commands", NULL };

static void
help_all(void)
{
	const cmdinfo_t	*ct;

	for (ct = cmdtab; ct < &cmdtab[ncmds]; ct++)
		help_oneline(ct->name, ct);
	dbprintf("\nUse 'help commandname' for extended help.\n");
}

static int
help_f(
	int		argc,
	char		**argv)
{
	const cmdinfo_t	*ct;

	if (argc == 0) {
		help_all();
		return 0;
	}
	ct = find_command(argv[0]);
	if (ct == NULL) {
		dbprintf("command %s not found\n", argv[0]);
		return 0;
	}
	help_onecmd(argv[0], ct);
	return 0;
}

void
help_init(void)
{
	add_command(&help_cmd);
}

static void
help_onecmd(
	const char	*cmd,
	const cmdinfo_t	*ct)
{
	help_oneline(cmd, ct);
	if (ct->help)
		ct->help();
}

static void
help_oneline(
	const char	*cmd,
	const cmdinfo_t	*ct)
{
	if (cmd)
		dbprintf("%s ", cmd);
	else {
		dbprintf("%s ", ct->name);
		if (ct->altname)
			dbprintf("(or %s) ", ct->altname);
	}
	if (ct->args)
		dbprintf("%s ", ct->args);
	dbprintf("-- %s\n", ct->oneline);
}

