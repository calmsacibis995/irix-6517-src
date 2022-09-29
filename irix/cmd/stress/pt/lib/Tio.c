
#include <unistd.h>
#include <stdio.h>
#include <stdarg.h>

#include <Tst.h>

#ifdef mips
#define write	__write
#endif

#define CONDITIONAL_PRINT(type) 	\
	if (!(TstOutput & type))	\
		return;			\
	PRINT

#define PRINT {							\
		va_list	ap;					\
		char	buf[512];				\
								\
		va_start(ap, format);				\
		write(2, buf, vsprintf(buf, format, ap));	\
		va_end(ap);					\
	}


int TstOutput = TST_ADMIN;	/* default is only error and admin output */

void
TstExit(const char *format, ...)
{
	PRINT;
	exit(1);
}


void
TstError(const char *format, ...)
{
	PRINT;	/* errors are always printed */
}


void
TstInfo(const char *format, ...)
{
	CONDITIONAL_PRINT(TST_INFO);
}


void
TstAdmin(const char *format, ...)
{
	CONDITIONAL_PRINT(TST_ADMIN);
}


void
TstTrace(const char *format, ...)
{
	CONDITIONAL_PRINT(TST_TRACE);
}


void
TstSetOutput(char *opt)
{
	char	c;

	if (!opt) return;

	TstOutput = 0;

	while (c = *opt++) {
		switch (c) {
		case 'I'	:
			TstOutput |= TST_INFO;
			break;
		case 'A'	:
			TstOutput |= TST_ADMIN;
			break;
		case 'T'	:
			TstOutput |= TST_TRACE;
			break;
		}
	}
}
