/*
 * AIM Suite III v3.1.1
 * (C) Copyright AIM Technology Inc. 1986,87,88,89,90,91,92.
 * All Rights Reserved.
 */

#ifndef	lint
	static char sccs_id[] = { " @(#) ttyinit.c:3.2 5/30/92 20:18:58" };
#endif

/*
 * create a list of available baud rates
 */

#include "ttytest.h"

struct bpair baudlst[] = {
    {0,B0},
#ifdef B50
    {50,B50},
#endif
#ifdef B75
    {75,B75},
#endif
#ifdef B110
    {110,B110},
#endif
#ifdef B150
    {150,B150},
#endif
#ifdef B200
    {200,B200},
#endif
#ifdef B300
    {300,B300},
#endif
#ifdef B600
    {600,B600},
#endif
#ifdef B1200
    {1200,B1200},
#endif
#ifdef B1800
    {1800,B1800},
#endif
#ifdef B2400
    {2400,B2400},
#endif
#ifdef B4800
    {4800,B4800},
#endif
#ifdef B9600
    {9600,B9600},
#endif
#ifdef B19200
    {19200,B19200},
#endif
    {-1,0}
};
