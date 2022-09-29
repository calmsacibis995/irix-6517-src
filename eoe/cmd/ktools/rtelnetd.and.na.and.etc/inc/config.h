/*
 *
 * Annex R9.0 src/inc/config.h Fri May  5 16:13:47 PDT 1995
 *
 * hardware "SGI" software "SYS_V" network "BSD"
 *
 */
 
#include <sys/types.h>
 
/* map function names */

 
#include <unistd.h>

#define need_sigmask
#define sigmask xylo_sigmask
#define NEED_SIGNED_CHARS

/* libannex linkage */
#include <port/libannex.h>

/* and additional port linkage */
#include <port/SYS_V.h>
