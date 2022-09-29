
/* unixperf - an x11perf-style Unix performance benchmark */

#include "unixperf.h"

/* test_regex.h */

extern char posix_txt[];

extern unsigned int DoLongLiteralStrRegcomp(void);
extern unsigned int DoLongDotStrRegcomp(void);
extern Bool InitLongLiteralStrRegex(Version version);
extern Bool InitLongDotStrRegex(Version version);
extern unsigned int DoPosixTxtRegexec(void);
extern void CleanupRegex(void);

