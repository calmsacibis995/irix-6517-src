/*
   util.h
*/

extern int errno;
extern char *optarg;
extern int optind, opterr, optopt;

/* Debug Routines ------------------------------------------------- */

#ifdef DEBUG

#include <malloc.h>
#include <sys/types.h>
#define MALLOPT() mallopt(M_DEBUG,1)
#include <math.h>
#define ZAP() free(malloc((unsigned int)(4 + (random()%128))));

#else

#define ZAP()
#define MALLOPT()

#endif

/* Utility Routines ----------------------------------------------- */

#include <stdio.h>
#include <ulocks.h>
extern usptr_t *SharedArena; /* set by init_globals() */

void error_exit(char *format, ...);
void error(char *format, ...);
void perror_exit(char *format, ...);
void perror2(char *format, ...);

/* Basic Types and Macros ---------------------------------------- */

typedef int bool;
#ifndef TRUE
#define TRUE (1==1)
#endif

#ifndef FALSE
#define FALSE (!TRUE)
#endif

#define countof(ARRAY) (sizeof(ARRAY)/sizeof((ARRAY)[0]))

#define max(a,b) ( ((a)>(b)) ? (a) : (b))
#define min(a,b) ( ((a)<(b)) ? (a) : (b))

#define USED(x) ((x)=(x))

typedef signed short int audioframe;


