#ifndef Log_included
#define Log_included

#include "bool.H"

#include <stdarg.h>

//  Log is a front end to syslog(3C).  It can also log to standard error.

class Log {

public:

    enum LogLevel { CRITICAL = 0, ERROR = 1, INFO = 2, DEBUG = 3 };

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
    static const char *name()		{ return program_name; }

private:

    static void vlog(LogLevel, const char *format, va_list);
    static void vfglog(LogLevel, const char *format, va_list args);

    static LogLevel level;
    static bool log_to_stderr;
    static const char *program_name;
    static bool syslog_open;

};

#endif /* !Log_included */
