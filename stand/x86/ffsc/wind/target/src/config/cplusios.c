/* cplusios.c - VxWorks iostreams class library initialization */

/* Copyright 1995 Wind River Systems, Inc. */

/*
modification history
--------------------
01b,07jun95,srh  correct duplicate naming problem (SPR 5100)
01a,14jun95,srh  written.
*/

/*
DESCRIPTION
This file is used to include the iostreams classes in the 
VxWorks build. The routines are only included when this file is 
included by usrConfig.c.

NOMANUAL
*/

#ifndef __INCcplusIosc
#define __INCcplusIosc

extern char __cstreams_o;
extern char __filebuf_o;
extern char __flt_o;
extern char __fstream_o;
extern char __generic_o;
extern char __intint_o;
extern char __ios_compat_o;
extern char __iosin_o;
extern char __iosout_o;
extern char __manip_o;
extern char __oldformat_o;
extern char __rawin_o;
extern char __sbuf_dbp_o;
extern char __stdiobuf_o;
extern char __stream_o;
extern char __streambuf_o;
extern char __strstream_o;
extern char __swstdio_o;

char * cplusIosFiles [] =
    {
    &__cstreams_o,
    &__filebuf_o,
    &__flt_o,
    &__fstream_o,
    &__generic_o,
    &__intint_o,
    &__ios_compat_o,
    &__iosin_o,
    &__iosout_o,
    &__manip_o,
    &__oldformat_o,
    &__rawin_o,
    &__sbuf_dbp_o,
    &__stdiobuf_o,
    &__stream_o,
    &__streambuf_o,
    &__strstream_o,
    &__swstdio_o
    };

#endif /* __INCcplusVxwc */
