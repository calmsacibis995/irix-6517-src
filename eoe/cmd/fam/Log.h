#ifndef Log_included
#define Log_included

#include "Boolean.h"
#include <stdarg.h>

//  Log is an augmented syslog(3C).  There are three log levels, DEBUG
//  INFO and LOG.  Logging can be done to the foreground (stderr) or
//  to the background (syslog).
//
//  Control interface:
//
//	debug(), info(), error()	control the log level.
//	foreground(), background()	control where log messages go.
//	name()				sets the program's name (i.e., fam)
//
//  Log interface:
//
//	debug(fmt, ...)		log at LOG_DEBUG level
//	info(fmt, ...)		log at LOG_INFO level
//	error(fmt, ...)		log at LOG_ERR level
//	perror(fmt, ...)	log at LOG_ERR, append system error string
//
//  All the log interface routines take printf-like arguments.  In
//  addition, they all recognizer "%m", which substitutes the system
//  error string.  "%m" works even when logging to foreground.
//
//  When NDEBUG is not defined, Log.C also implements a replacement
//  for the libc function __assert().  The replacement function writes
//  the failed assertion message to the current log (i.e., stderr or
//  syslog), then sets its uid to 0, creates a directory in /usr/tmp,
//  logs the name of the directory, and chdir() to it so the core file
//  can be found.

class Log {

public:

    enum LogLevel { CRITICAL = 0, ERROR = 1, INFO = 2, DEBUG = 3 };

#ifdef NO_LEAKS
    Log();
    ~Log();
#endif

    // Message logging functions.  Three different priorities.

    static void debug(const char *fmt, ...);
    static void info(const char *fmt, ...);
    static void error(const char *fmt, ...);
    static void critical(const char *fmt, ...);

    // perror() is like error() but appends system error string.

    static void perror(const char *format, ...);

    // Control functions

    static void debug();
    static void info();
    static void error();

    static void foreground();
    static void background();

    static void name(const char *);

private:

    static void vlog(LogLevel, const char *format, va_list);
    static void vfglog(LogLevel, const char *format, va_list args);

    static LogLevel level;
    static Boolean log_to_stderr;
    static const char *program_name;
    static Boolean syslog_open;
    static unsigned count;

};

#ifdef NO_LEAKS
static Log Log_instance;
#endif

#endif /* !Log_included */
