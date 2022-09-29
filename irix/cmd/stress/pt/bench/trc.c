/*
 * A simple variable-verbosity output facility. Avoids stdio thread-safety
 * overheads, but since write can block, we still have to be careful.
 */

#include <unistd.h>
#include <stdarg.h>
#include <pthread.h>
#include "trc.h"

static int v;

void trc_set_verbosity(int level) { v = level; }

#define BUFSZ 256
static int our_printf(int fd, char* fmt, va_list args)
{
	char buf[BUFSZ];
	int retval = vsnprintf(buf, sizeof(buf), fmt, args);
	__write(fd, buf, strlen(buf));
	return retval;
}

#define PRINT_COND(level,fmt) \
	va_list args; \
	int retval = 0; \
	char buf[BUFSZ]; \
	snprintf(buf, sizeof(buf), "thr0x%08x:\t%s", pthread_self(), fmt); \
	 \
	if ( level <= v ) { \
		va_start(args, fmt); \
		retval = our_printf(STDOUT_FILENO, buf, args);  \
		va_end(args); \
	} \
 \
	return retval;

int trc(int level, char* fmt, ...)
{
	PRINT_COND(level, fmt)
}

#define PRINT_TYPE(name, level) \
int name(char* fmt, ...) { PRINT_COND(level, fmt) }

PRINT_TYPE(trc_result, TRC_RESULT)
PRINT_TYPE(trc_info, TRC_INFO)
PRINT_TYPE(trc_vrb, TRC_VRB)

int trc_print(int fd, char* fmt, ...)
{
	va_list args;
	int retval;
	va_start(args, fmt);
	retval = our_printf(STDERR_FILENO, fmt, args);
	va_end(args);
	return retval;
}

