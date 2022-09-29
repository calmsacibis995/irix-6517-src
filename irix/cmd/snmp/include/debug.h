#ifndef DEBUG_H
#define DEBUG_H
/*
 * Copyright 1988 Silicon Graphics, Inc.  All rights reserved.
 *
 * Debugging support: assertion and metering macros.
 */
#include <assert.h>
#ifdef sun
#include <stdio.h>
#endif

#ifdef METERING
#define	METER(x)	x
#else
#define	METER(x)
#endif

#endif
