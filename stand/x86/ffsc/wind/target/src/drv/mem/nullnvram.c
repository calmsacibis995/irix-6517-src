/* nullNvRam.c - NULL non-volatile RAM library */

/* Copyright 1984-1992 Wind River Systems, Inc. */
#include "copyright_wrs.h"

/*
modification history
--------------------
01d,15dec92,caf  reinstated version 01b.
01c,21oct92,ccc  fixed ansi warnings.
01b,17jul92,caf  changed nullNvram.c to nullNvRam.c.
01a,26jun92,caf  created.
*/

/*
DESCRIPTION
This library contains non-volatile RAM manipulation routines for targets
lacking non-volatile RAM.  Read and write routines that return ERROR
are included.

The macro NV_RAM_SIZE should be defined as NONE for targets lacking
non-volatile RAM.
*/

/******************************************************************************
*
* sysNvRamGet - get the contents of non-volatile RAM
*
* This routine copies the contents of non-volatile memory into a specified
* string.  The string will be terminated with an EOS.
*
* NOTE: This routine has no effect, since there is no non-volatile RAM.
*
* RETURNS: ERROR, since there is no non-volatile RAM.
*
* SEE ALSO: sysNvRamSet()
*/

STATUS sysNvRamGet
    (
    char *string,    /* where to copy non-volatile RAM    */
    int strLen,      /* maximum number of bytes to copy   */
    int offset       /* byte offset into non-volatile RAM */
    )
    {
    return (ERROR);
    }

/*******************************************************************************
*
* sysNvRamSet - write to non-volatile RAM
*
* This routine copies a specified string into non-volatile RAM.
*
* NOTE: This routine has no effect, since there is no non-volatile RAM.
*
* RETURNS: ERROR, since there is no non-volatile RAM.
*
* SEE ALSO: sysNvRamGet()
*/

STATUS sysNvRamSet
    (
    char *string,     /* string to be copied into non-volatile RAM */
    int strLen,       /* maximum number of bytes to copy           */
    int offset        /* byte offset into non-volatile RAM         */
    )
    {
    return (ERROR);
    }
