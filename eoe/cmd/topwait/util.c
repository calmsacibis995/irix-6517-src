#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "util.h"

#define	roundup(x, y)	((((x)+((y)-1))/(y))*(y))

int	numb_processors = 1;
const char* appName = "";
const char* hostname = "localhost";

static void
proc_syntax(const char* what)
{
    fatal("syntax error in processor list; %s", what);
}

void
parse_proc_str(const char* arg, uint64_t mask[])
{
    const char* cp = arg;

    memset(mask, 0, roundup(numb_processors,64)>>3);
    while (*cp) {
	int m, n;

	if (!isdigit(*cp))
	    proc_syntax("expecting CPU number");
	m = 0;
	do {
	    m = 10*m + (*cp-'0');
	} while (isdigit(*++cp));
	if (*cp == '-') {
	    cp++;
	    if (isdigit(*cp)) {
		n = 0;
		do {
		    n = 10*n + (*cp-'0');
		} while (isdigit(*++cp));
	    } else if (*cp == '\0') {		/* e.g. 2- */
		n = numb_processors-1;
	    } else
		proc_syntax("expecting CPU number");
	} else if (*cp == ',') {
	    cp++;
	    n = m;
	} else if (*cp == '\0') {
	    n = m;
	} else
	    proc_syntax("expecting ',' or '-' after CPU number");
	if (m > n)
	    fatal("inverted CPU range, %d > %d", m, n);
	if (m >= numb_processors)
	    fatal("%d: CPU number out of range; %s has only %d processors",
		m, hostname, numb_processors);
	for (; m <= n; m++)
	    mask[m>>6] |= ((uint64_t) 1)<<(m&0x3f);
    }
}

static void
event_syntax(const char* what)
{
    fatal("syntax error in event list; %s", what);
}

typedef struct {
    const char* name;
    uint64_t	mask;
} maskname_t;

static uint64_t
event_mask_name(const char* tp, size_t n)
{
#define	N(a)	(sizeof (a) / sizeof (a[0]))
    static maskname_t masks[] = {
	{ "debug",	RTMON_DEBUG },
	{ "signal",	RTMON_SIGNAL },
	{ "syscall",	RTMON_SYSCALL },
	{ "task",	RTMON_TASK },
	{ "intr",	RTMON_INTR },
	{ "framesched",	RTMON_FRAMESCHED },
	{ "profile",	RTMON_PROFILE },
	{ "vm",		RTMON_VM },
	{ "scheduler",	RTMON_SCHEDULER },
	{ "disk",	RTMON_DISK },
	{ "netsched",	RTMON_NETSCHED },
	{ "netflow",	RTMON_NETFLOW },

	{ "all",	RTMON_ALL },
	{ "network",	RTMON_NETSCHED|RTMON_NETFLOW },
	{ "io",		RTMON_DISK|RTMON_INTR },
	{ "par",	RTMON_PAR },
    };
    maskname_t* mp;

    for (mp = masks; mp < &masks[N(masks)]; mp++)
	if (strncasecmp(mp->name, tp, n) == 0)
	    return (mp->mask);
    fatal("%.*s: Uknown event mask name", n, tp);
    /*NOTREACHED*/
#undef N
}

uint64_t
parse_event_str(const char* arg)
{
    const char* cp = arg;
    uint64_t mask = 0;
    int incl = 1;
    while (*cp) {
	if (isalpha(*cp)) {
	    const char* tp = cp;
	    do {
		*cp++;
	    } while (isalpha(*cp));
	    if (incl)
		mask |= event_mask_name(tp, cp-tp);
	    else
		mask &= ~event_mask_name(tp, cp-tp);
	} else
	    event_syntax("expecting event mask name");
	if (*cp == ',' || *cp == '|' || *cp == '+' || *cp == '-')
	    incl = (*cp++ != '-');
	else if (*cp != '\0')
	    event_syntax("expecting ',' after event mask name");
    }
    return (mask);
}

void
fatal(const char* fmt, ...)
{
    va_list ap;
    fprintf(stderr, "%s: ", appName);
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
    exit(EXIT_FAILURE);
}

void
log_event(tstamp_event_entry_t *ev)
{
	static FILE *fp = NULL;
	static int initialized = 0;

 	if (!initialized) {
		fp = fopen("default", "w");
		initialized = 1;
	}

	if (fp != NULL) {
		fwrite(ev, sizeof(*ev), 1, fp);
	}
}
