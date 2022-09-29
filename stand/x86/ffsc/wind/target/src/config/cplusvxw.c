/* cplusVxw.c - VxWorks wrapper class initialization */

/* Copyright 1995 Wind River Systems, Inc. */

/*
modification history
--------------------
01a,14jun95,srh  written.
*/

/*
DESCRIPTION
This file is used to include the VxWorks wrapper classes in the 
VxWorks build. The routines are only included when this file is 
included by usrConfig.c.

NOMANUAL
*/

#ifndef __INCcplusVxwc
#define __INCcplusVxwc

extern char __vxwErr_o;
extern char __vxwLoadLib_o;
extern char __vxwLstLib_o;
extern char __vxwMemPartLib_o;
extern char __vxwMsgQLib_o;
extern char __vxwRngLib_o;
extern char __vxwSemLib_o;
extern char __vxwSymLib_o;
extern char __vxwTaskLib_o;
extern char __vxwWdLib_o;

char * cplusVxwFiles [] =
    {
    &__vxwErr_o,
    &__vxwLoadLib_o,
    &__vxwLstLib_o,
    &__vxwMemPartLib_o,
    &__vxwMsgQLib_o,
    &__vxwRngLib_o,
    &__vxwSemLib_o,
    &__vxwSymLib_o,
    &__vxwTaskLib_o,
    &__vxwWdLib_o
    };

#endif /* &__INCcplusVxwc */
