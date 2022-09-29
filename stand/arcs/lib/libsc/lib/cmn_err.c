/*
 * cmn_err.c - System V like error printer for standalone programs
 *
 * $Revision: 1.2 $
 */
#include <sys/cmn_err.h>
#include <stdarg.h>
#include <libsc.h>

void
cmn_err(int level, char *fmt, ...)
{
	va_list ap;

	/*CONSTCOND*/
	va_start(ap, fmt);
	if(level == CE_PANIC) {
		/* handle via panic to handle regdump, double panic, etc. */
		char buf[256];
		vsprintf(buf, fmt, ap);
		panic("%s", buf);
		va_end(ap);
		/* NOTREACHED */
	}
	vprintf(fmt, ap);
	va_end(ap);
}
