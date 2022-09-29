#include <stdarg.h>		/* for vprintf() */
#include <stdio.h>
#include "debug.h"

int __debug = 0;

void DBG(int flag, char *format, ...)
{
  va_list ap;
  char tempstr[100];

  if (flag & __debug) {
    va_start(ap, format);
    sprintf(tempstr, "%s", format);
    vprintf(tempstr, ap);
    va_end(ap);
  }
}

void fatal(char *format, ...)
{
  static int fatalizing=0;
  va_list ap;
  char tempstr[100];

  if (fatalizing)
    return;

  fatalizing = 1;

  va_start(ap, format);
  sprintf(tempstr, "FATAL ERROR : %s", format);
  vfprintf(stderr, tempstr, ap);
  fflush(stderr);
  va_end(ap);
  cleanup();
  exit(1);
}
