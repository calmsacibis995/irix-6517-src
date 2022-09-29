
/* unixperf - an x11perf-style Unix performance benchmark */

/* test_regex.c tests the speed of POSIX regex/regcomp pattern matching. */

/* These regexec/regcomp performance tests are inspired by Tom Lord's "Regexp
   Library Cook-off" in the rx-1.3 distribution. */

#include "unixperf.h"
#include "test_regex.h"

#ifndef HAS_NO_REGEX

#include <regex.h>

static char longLiteralStr[] = "back-tracking oriented stream-of-solution functions";
static char longDotStr[] = "back-tracking .* functions";

unsigned int
DoLongLiteralStrRegcomp(void)
{
  regex_t r;

  regcomp(&r, longLiteralStr, REG_EXTENDED);
  regfree(&r);
  regcomp(&r, longLiteralStr, REG_EXTENDED);
  regfree(&r);
  regcomp(&r, longLiteralStr, REG_EXTENDED);
  regfree(&r);
  regcomp(&r, longLiteralStr, REG_EXTENDED);
  regfree(&r);
  return 4;
}

unsigned int
DoLongDotStrRegcomp(void)
{
  regex_t r;

  regcomp(&r, longDotStr, REG_EXTENDED);
  regfree(&r);
  regcomp(&r, longDotStr, REG_EXTENDED);
  regfree(&r);
  regcomp(&r, longDotStr, REG_EXTENDED);
  regfree(&r);
  regcomp(&r, longDotStr, REG_EXTENDED);
  regfree(&r);
  return 4;
}

static regex_t r;

Bool
InitLongLiteralStrRegex(Version version)
{
  regcomp(&r, longLiteralStr, REG_EXTENDED);
  return TRUE;
}

Bool
InitLongDotStrRegex(Version version)
{
  regcomp(&r, longDotStr, REG_EXTENDED);
  return TRUE;
}

unsigned int
DoPosixTxtRegexec(void)
{
  regmatch_t regs[10];

  regexec (&r, posix_txt, 10, regs, 0);
  return 1;
}

void
CleanupRegex(void)
{
  regfree(&r);
}

#endif /* NO_REGEX */
