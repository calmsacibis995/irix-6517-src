/*
 * AIM Suite III v3.1.1
 * (C) Copyright AIM Technology Inc. 1986,87,88,89,90,91,92.
 * All Rights Reserved.
 */

#ifndef	ttytest_h
#	define ttytest_h " @(#) ttytest.h:3.2 5/30/92 20:18:59"
#endif

/* header file for the tty exerciser portion of the benchmark */

#define DEAD 0				/* driver asked to stop */
#define GOOD 1				/* everything seems to be ok */

#define SEQLEN 32

#define KILLTTY ttyctl(-1,0L,0L,0,0L)
#define PAUSETTY(data) ttyctl(0,0L,0L,0,(data))

struct ttdata  {			/* the information returned */
    long errors;
    long num_rd,num_wt;
};

struct bpair {
    int val;
    int lbl;
};

#ifdef SYS5

#include <sys/termio.h>
typedef struct termio ttyinf;

#else		/* V7 or BSD UNIX */

#include <sgtty.h>
typedef struct sgttyb ttyinf;

#endif

#ifndef V7
#define WFLAG (O_WRONLY|O_NDELAY)
#define RFLAG (O_RDONLY|O_NDELAY)
#else
#define WFLAG 1
#define RFLAG 0 
#endif

#ifndef SYS5
#define TERMGET TIOCGETP
#define TERMSET TIOCSETP
#define TERMFLUSH TIOCFLUSH
#else
#define TERMGET TCGETA
#define TERMSET TCSETAF
#define TERMFLUSH TCFLSH
#endif
