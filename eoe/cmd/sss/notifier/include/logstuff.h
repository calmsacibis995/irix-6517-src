#ifndef LOGSTUFF_H
#define LOGSTUFF_H

#define logfileloc "/var/esp/ssNotifylog"

#include <time.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include <sys/types.h>

/* #include "ssNotify.h" */

int logstuff(int errnum, char *message);

#endif
