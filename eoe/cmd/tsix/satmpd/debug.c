#ident "$Revision: 1.1 $"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#define DEBUG_IMPL
#include "debug.h"
#include "parse.h"

static struct debug_opts {
	char *name;
	unsigned int value;
} debug_options[] = {
	{"STARTUP", DEBUG_STARTUP},
	{"FILE_OPEN", DEBUG_FILE_OPEN},
	{"DIR_OPEN", DEBUG_DIR_OPEN},
	{"OPEN_FAIL", DEBUG_OPEN_FAIL},
	{"OPENDIR_FAIL", DEBUG_OPENDIR_FAIL},
	{"PROTOCOL", DEBUG_PROTOCOL},
	{"ALL", DEBUG_ALL_ON},
	{(char *) NULL, DEBUG_ALL_OFF},
};

static int
iscomma (int c)
{
	return (c == ',');
}

char *
name_debug_opts (void)
{
	static char names[256], *np = names;
	struct debug_opts *dop;

	*np = '\0';
	for (dop = debug_options; dop->name != NULL; dop++)
	{
		strcpy (np, dop->name);
		np += strlen (np);
		strcat (np++, ",");
	}
	*(np - 1) = '\0';
	return (names);
}

int
parse_debug_opts (const char *arg)
{
	char **options, **opt_names;

	options = parse_line (arg, (pl_quote_func) NULL, iscomma,
			      (unsigned int *) NULL);
	if (options == NULL)
		return (1);

	for (opt_names = options; *opt_names != NULL; opt_names++)
	{
		struct debug_opts *dop;

		for (dop = debug_options; dop->name != NULL; dop++)
		{
			if (strcmp (dop->name, *opt_names) == 0)
			{
				add_debug_state (dop->value);
				break;
			}
		}

		if (dop->name == NULL)
		{
			parse_free (options);
			return (1);
		}
	}

	parse_free (options);
	return (0);
}

void
debug_set_log (const char *file)
{
	/* close old log fp */
	fclose (debug_fp);

	/* open new log fp */
	debug_fp = fopen (file, "a+");
	if (debug_fp == (FILE *) NULL)
	{
		perror (file);
		exit (1);
	}
}

void
debug_print (const char *fmt, ...)
{
	va_list ap;

	va_start (ap, fmt);
	vfprintf (debug_fp, fmt, ap);
	va_end (ap);
	fflush (debug_fp);
}
