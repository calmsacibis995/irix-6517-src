#include "Log.h"

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <unistd.h>

Log::LogLevel Log::level         = ERROR;
Boolean       Log::log_to_stderr = false;
const char   *Log::program_name  = "fam";
Boolean       Log::syslog_open   = false;
unsigned      Log::count         = 0;

void
Log::debug()
{
    level = DEBUG;
    info("log level is LOG_DEBUG");
}

void
Log::info()
{
    level = INFO;
    info("log level is LOG_INFO");
}

void
Log::error()
{
    level = INFO;
    info("log level is LOG_ERR");
    level = ERROR;
}

void
Log::foreground()
{
    log_to_stderr = true;
    if (syslog_open)
    {   closelog();
	syslog_open = false;
    }
}

void
Log::background()
{
    log_to_stderr = false;
}

void
Log::name(const char *newname)
{
    program_name = newname;
}

void
Log::debug(const char *fmt, ...)
{
    if (level >= DEBUG)
    {
	va_list a;
	va_start(a, fmt);
	vlog(DEBUG, fmt, a);
	va_end(a);
    }
}

void
Log::info(const char *fmt, ...)
{
    if (level >= INFO)
    {
	va_list a;
	va_start(a, fmt);
	vlog(INFO, fmt, a);
	va_end(a);
    }
}

void
Log::error(const char *fmt, ...)
{
    if (level >= ERROR)
    {
	va_list a;
	va_start(a, fmt);
	vlog(ERROR, fmt, a);
	va_end(a);
    }
}

void
Log::critical(const char *fmt, ...)
{
    if (level >= CRITICAL)
    {
	va_list a;
	va_start(a, fmt);
	vlog(CRITICAL, fmt, a);
	va_end(a);
    }
}

void
Log::perror(const char *format, ...)
{
    if (level >= ERROR)
    {   char buf[BUFSIZ];
	(void) strcpy(buf, format);
	(void) strcat(buf, ": %m");
	va_list args;
	va_start(args, format);
	vlog(ERROR, buf, args);
	va_end(args);
    }
}

void
Log::vlog(LogLevel l, const char *format, va_list args)
{
    if (level >= l)
	if (log_to_stderr)
	    vfglog(l, format, args);
	else
	{   if (!syslog_open)
	    {   openlog(program_name, 0, LOG_DAEMON);
		syslog_open = true;
	    }
	    int ll;
	    switch (l)
	    {
	    case DEBUG:
		ll = LOG_DEBUG; break;
	    case INFO:
		ll = LOG_INFO;  break;
	    case ERROR:
		ll = LOG_ERR;   break;
	    case CRITICAL:
		ll = LOG_CRIT;   break;
	    default:
		ll = LOG_CRIT;  break;	// Why not?
	    }
	    vsyslog(ll, format, args);
	}
}

void
Log::vfglog(LogLevel l, const char *format, va_list args)
{
    if (level >= l)
    {   char buf[BUFSIZ], *p = buf;
	while (*format)
	{   if (format[0] == '%')
	    {   if (format[1] == 'm')
		{   p += strlen(strcpy(p, strerror(errno)));
		    format += 2;
		    continue;
		}
		else
		    *p++ = *format++;
	    }
	    *p++ = *format++;
	}
	(void) strcpy(p, "\n");
//	assert(strchr(buf, '\n') == p);
	(void) fprintf(stderr, "%s: ", program_name);
	(void) vfprintf(stderr, buf, args);
    }
}

#ifndef NDEBUG

//  New back end for assert() will log to syslog, put core file
//  in known directory.

void __assert(const char *msg, const char *file, int line)
{
    char dirname[40];
    (void) sprintf(dirname, "/usr/tmp/fam.%d", getpid());
    Log::error("Assertion failed at %s line %d: %s", file, line, msg);
    Log::error("Dumping core in %s/core", dirname);

    if (setreuid(0, 0) < 0)
	Log::perror("setreuid");
    if (mkdir(dirname, 0755) < 0)
	Log::perror("mkdir");
    if (chdir(dirname) < 0)
	Log::perror("chdir");
    struct rlimit rl = { RLIM_INFINITY, RLIM_INFINITY };
    if (setrlimit(RLIMIT_CORE, &rl) < 0)
	Log::perror("setrlimit(RLIMIT_CORE)");
    abort();
}

#endif /* !NDEBUG */

#ifdef NO_LEAKS

Log::Log()
{
    count++;
}

Log::~Log()
{
    if (!--count)
    {   if (syslog_open)
	{   closelog();
	    syslog_open = false;
	}
    }
}

#endif /* NO_LEAKS */

#ifdef UNIT_TEST_Log

#include <fcntl.h>
#include <sys/stat.h>

int
main()
{
    Log::name("unit test");
    Log::debug();
    Log::foreground();
    Log::debug("De bug is in %s.", "de rug");
    Log::info("Thank %s for sharing %s.", "you", "that");
    Log::error("The %s in %s falls.", "rain", "Spain");
    if (open("/foo%bar", 0) < 0)
	Log::perror("/foo%c%s", '%', "bar");
    if (chmod("/", 0777) < 0)
	Log::error("%m on chmod(\"%s\", 0%o)", "/", 0777);
    return 0;
}

#endif /* UNIT_TEST_Log */
