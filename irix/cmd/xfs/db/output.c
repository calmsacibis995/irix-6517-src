#ident "$Revision: 1.2 $"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "command.h"
#include "output.h"
#include "sig.h"
#include "malloc.h"
#include "init.h"

static int	log_f(int argc, char **argv);

static const cmdinfo_t	log_cmd =
	{ "log", NULL, log_f, 0, 2, 0, "[stop|start <filename>]",
	  "start or stop logging to a file", NULL };

int		dbprefix;
static FILE	*log_file;
static char	*log_file_name;

int
dbprintf(const char *fmt, ...)
{
	va_list	ap;
	int	i;

	if (seenint())
		return 0;
	va_start(ap, fmt);
	blockint();
	i = 0;
	if (dbprefix)
		i += printf("%s: ", fsdevice);
	i += vprintf(fmt, ap);
	unblockint();
	va_end(ap);
	if (log_file) {
		va_start(ap, fmt);
		vfprintf(log_file, fmt, ap);
		va_end(ap);
	}
	return i;
}

static int
log_f(
	int		argc,
	char		**argv)
{
	if (argc == 0) {
		if (log_file)
			dbprintf("logging to %s\n", log_file_name);
		else
			dbprintf("no log file\n");
	} else if (argc == 1 && strcmp(argv[0], "stop") == 0) {
		if (log_file) {
			xfree(log_file_name);
			fclose(log_file);
			log_file = NULL;
		} else
			dbprintf("no log file\n");
	} else if (argc == 2 && strcmp(argv[0], "start") == 0) {
		if (log_file)
			dbprintf("already logging to %s\n", log_file_name);
		else {
			log_file = fopen(argv[1], "a");
			if (log_file == NULL)
				dbprintf("can't open %s for writing\n",
					argv[1]);
			else
				log_file_name = xstrdup(argv[0]);
		}
	} else
		dbprintf("bad log command, ignored\n");
	return 0;
}

void
logprintf(const char *fmt, ...)
{
	va_list	ap;

	if (log_file) {
		va_start(ap, fmt);
		(void)vfprintf(log_file, fmt, ap);
		va_end(ap);
	}
}

void
output_init(void)
{
	add_command(&log_cmd);
}
