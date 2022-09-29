/*
 * define _libsc_private here such that it will work correctly for
 * MP machine with initialized data in PROM, e.g. IP30.  IP30 version
 * of _libsc_private is defined in IP30prom/IP30.c
 */

#ident "$Revision: 1.1 $"

#include <libsc.h>

static libsc_private_t tmpbuf;
libsc_private_t *_libsc_private = &tmpbuf;
