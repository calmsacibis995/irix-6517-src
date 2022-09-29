/* unldLib.h - header for unload library */

/* Copyright 1992 Wind River Systems, Inc. */
/*
modification history
--------------------
01f,22aug93,jmm  added option type to supress breakpoint deletion
01e,16aug93,jmm  added new errno - S_unldLib_TEXT_IN_USE
01d,30oct92,jmm  added prototype for reld() (spr 1716)
01c,22sep92,rrr  added support for c++
01b,18sep92,jcf  added include of moduleLib.h.
01a,14may92,jmm  written.
*/

#ifndef __INCunldLibh
#define __INCunldLibh

#ifdef __cplusplus
extern "C" {
#endif

#include "modulelib.h"

/* unldLib Status Codes */

#define S_unldLib_MODULE_NOT_FOUND             (M_unldLib | 1)
#define S_unldLib_TEXT_IN_USE                  (M_unldLib | 2)

/* options for unld */

#define UNLD_KEEP_BREAKPOINTS  1 /* don't delete breakpoints from unld() */

/* function declarations */

#if defined(__STDC__) || defined(__cplusplus)

extern STATUS unld (void *name, int options);
extern STATUS unldByNameAndPath (char *name, char *path, int options);
extern STATUS unldByGroup (UINT16 group, int options);
extern STATUS unldByModuleId (MODULE_ID moduleId, int options);
MODULE_ID reld (void * nameOrId, int options);

#else   /* __STDC__ */

extern STATUS unld ();
extern STATUS unldByNameAndPath ();
extern STATUS unldByGroup ();
extern STATUS unldByModuleId ();
MODULE_ID reld ();

#endif  /* __STDC__ */

#ifdef __cplusplus
}
#endif

#endif /* __INCunldLibh */
