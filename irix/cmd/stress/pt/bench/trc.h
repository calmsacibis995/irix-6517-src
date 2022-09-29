/*
 * Simple interface for doing varying-level verbosity output.
 */
#ifndef TRACE_H_
#define TRACE_H_

#define BnchPrInfo trc_result
#include <Bnch.h>

extern void trc_set_verbosity(int level);
extern int trc(int level, char* fmt, ... );
extern int trc_result(char* fmt, ...);
extern int trc_info(char* fmt, ... );
extern int trc_vrb(char* fmt, ...);
extern int trc_print(int fd, char* fmt, ...);

#define TRC_RESULT	1	/* test results (i.e., measurements) */
#define TRC_INFO	2	/* miscellaneous trace information */
#define TRC_VRB		3	/* really verbose messages */

#endif
