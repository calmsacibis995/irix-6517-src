#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <fcntl.h>
#include <syslog.h>
#include <unistd.h>

#define SYSLOG_MAX	10	/* Max syslog lines written per invocation */

static char *prog_prefix = "mmscd: ";
static int SilentEnable;
static int syslog_lines;
static int debug_fd;

void debug_enable(char *filename)
{
    if (filename == 0) {
	if (debug_fd)
	    close(debug_fd);
	debug_fd = 0;
    } else if ((debug_fd = open(filename,
				O_CREAT | O_TRUNC | O_WRONLY, 0666)) < 0) {
	fprintf(stderr, "Could not open %s for debug output\n", filename);
	exit(1);
    }
}

void silent_enable(int flag)
{
    SilentEnable = flag;
}

void debug(char *fmt, ...)
{
    va_list		ap;
    char		buf[256];

    if (debug_fd) {
	va_start(ap,fmt);
	strcpy(buf, "debug: ");
	vsprintf(buf + strlen(buf), fmt, ap);
#if 0
	syslog(LOG_DEBUG, buf);		/* Too much for this */
#else
	write(debug_fd, buf, strlen(buf));
#endif
    }
}

void warning(char *fmt, ...)
{
    va_list		ap;
    char		buf[256];

    if (! SilentEnable) {
	va_start(ap,fmt);
	strcpy(buf, "warning: ");
	vsprintf(buf + strlen(buf), fmt, ap);

	if (debug_fd) {
	    write(debug_fd, prog_prefix, strlen(prog_prefix));
	    write(debug_fd, buf, strlen(buf));
	}

	if (syslog_lines++ < SYSLOG_MAX)
	    syslog(LOG_WARNING, buf);
    }
}

void fatal(char *fmt, ...)
{
    va_list		ap;
    char		buf[256];
    extern void		cleanup(void);

    va_start(ap,fmt);
    strcpy(buf, "fatal: ");
    vsprintf(buf + strlen(buf), fmt, ap);

    if (debug_fd) {
	write(debug_fd, prog_prefix, strlen(prog_prefix));
	write(debug_fd, buf, strlen(buf));
    }

    if (syslog_lines++ < SYSLOG_MAX)
	syslog(LOG_ERR, buf);

    cleanup();

    exit(1);
}

void perr(char *fmt, ...)
{
    va_list		ap;

    va_start(ap,fmt);
    fputs(prog_prefix, stderr);
    vfprintf(stderr, fmt, ap);
}
