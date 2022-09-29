/* sigevent.h - sigevent structure, needed by several header files */

/* Copyright 1984-1992 Wind River Systems, Inc. */

/*
modification history
--------------------
02c,09nov93,rrr  update to posix 1003.4 draft 14
02b,22sep92,rrr  added support for c++
02a,04jul92,jcf  cleaned up.
01b,26may92,rrr  the tree shuffle
01a,19feb92,rrr  written from posix spec
*/

#ifndef __INCsigeventh
#define __INCsigeventh

#ifdef __cplusplus
extern "C" {
#endif

union sigval
    {
    int			sival_int;
    void		*sival_ptr;
    };

struct sigevent
    {
    int			sigev_signo;
    union sigval	sigev_value;
    int			sigev_notify;
    };

#ifdef __cplusplus
}
#endif

#endif /* __INCsigeventh */
