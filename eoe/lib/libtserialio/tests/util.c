/*
   util.c
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include <stdarg.h>

#include <unistd.h>
#include <bstring.h>
#include <sys/types.h>
#include <sys/resource.h>
#include <sys/mman.h>
#include <sys/statfs.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/prctl.h>
#include <sys/schedctl.h>
#include <assert.h>
#include <signal.h>

#include <fcntl.h>
#include <ulocks.h>
#include <errno.h>
#include <stropts.h>

#include "util.h"

/* Utility Routines ----------------------------------------------- */

void error_exit(char *format, ...)
{
  va_list ap;
  
  va_start(ap, format);
  vfprintf( stderr, format, ap );
  va_end(ap);

  fprintf( stderr, "\n" );
  exit(2);
}

void error(char *format, ...)
{
  va_list ap;
  
  va_start(ap, format);
  vfprintf( stderr, format, ap );
  va_end(ap);

  fprintf( stderr, "\n" );
}

void perror_exit(char *format, ...)
{
  va_list ap;
  
  va_start(ap, format);
  vfprintf( stderr, format, ap );
  va_end(ap);

  fprintf(stderr, ": ");
  perror("");

  fprintf( stderr, "\n" );
  exit(2);
}

void perror2(char *format, ...)
{
  va_list ap;
  
  va_start(ap, format);
  vfprintf( stderr, format, ap );
  va_end(ap);

  fprintf(stderr, ": ");
  perror("");

  fprintf( stderr, "\n" );
}

