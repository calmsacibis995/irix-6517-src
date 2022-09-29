/* loadLib.h - object module loader library header */

/* Copyright 1984-1992 Wind River Systems, Inc. */
/*
modification history
--------------------
01s,22sep92,rrr  added support for c++
01r,22jul92,jmm  removed NO_TRACK_MODULE define
                 moved SEG_INFO here from various loaders
		 added prototypes for addSegNames and loadSegmentsAllocate
01q,21jul92,jmm  added HIDDEN_MODULE define
01p,18jun92,ajm  made object module independant
01o,26may92,rrr  the tree shuffle
01n,14may92,ajm  rewritten for object module independent loadLib
		  updated copyright
01m,04oct91,rrr  passed through the ansification filter
		  -fixed #else and #endif
		  -changed copyright notice
01l,02aug91,ajm  added mips specific error entries
01k,24mar91,del  added I960 defines.
01j,05oct90,dnw  deleted private routines.
01i,05oct90,shl  added ANSI function prototypes.
                 made #endif ANSI style.
                 added copyright notice.
01h,07aug90,shl  added INCloadLibh to #endif.
01g,05jun88,dnw  changed ldLib to loadLib.
01f,07aug87,ecs  added LD_NO_ADDRESS.
01e,24dec86,gae  changed stsLib.h to vwModNum.h.
01d,10aug84,dnw  removed unused status code: S_ldLib_UNDEFINED_SYMBOL.
01c,07aug84,ecs  added include of stsLib.h
		 added status codes
		 added inclusion test
01b,29jun84,ecs  changed values of *_SYMBOLS so default would be GLOBAL_SYMBOLS
01a,27apr84,dnw  written
*/

#ifndef __INCloadLibh
#define __INCloadLibh

#ifdef __cplusplus
extern "C" {
#endif

#include "vwmodnum.h"
#include "symlib.h"
#include "modulelib.h"

/* status codes */

#define S_loadLib_ROUTINE_NOT_INSTALLED		(M_loadLib | 1)
#define S_loadLib_TOO_MANY_SYMBOLS		(M_loadLib | 2)

#define NO_SYMBOLS      -1
#define GLOBAL_SYMBOLS  0
#define ALL_SYMBOLS     1

/* new load flags */
#define LOAD_NO_SYMBOLS	        2
#define LOAD_LOCAL_SYMBOLS	4
#define LOAD_GLOBAL_SYMBOLS	8
#define LOAD_ALL_SYMBOLS	(LOAD_LOCAL_SYMBOLS | LOAD_GLOBAL_SYMBOLS)
#define HIDDEN_MODULE	        16 /* Don't display module from moduleShow() */

#define LD_NO_ADDRESS   ((char *) NONE)

/* data structures */

extern FUNCPTR loadRoutine;

typedef struct
    {
    char * addrText;	/* text segment address */
    char * addrData;	/* data segment address */
    char * addrBss;	/* bss segment address */
    UINT   sizeText;	/* text segment size */
    UINT   sizeProtectedText;	/* protected text segment size */
    UINT   sizeData;	/* data segment size */
    UINT   sizeBss;	/* bss segment size */
    int	   flagsText;	/* text flags for module */
    int	   flagsData;	/* data flags for module */
    int	   flagsBss;	/* bss flags for module */
    } SEG_INFO;

/* function declarations */

#if defined(__STDC__) || defined(__cplusplus)

extern MODULE_ID loadModule (int fd, int symFlag);
extern MODULE_ID loadModuleAt (int fd, int symFlag, char **ppText,
			       char **ppData, char **ppBss);
extern MODULE_ID loadModuleAtSym (int fd, int symFlag, char **ppText,
				  char **ppData, char **ppBss,
				  SYMTAB_ID symTbl);
extern MODULE_ID loadModuleGet (char *fileName, int format, int *symFlag);
extern void addSegNames (int fd, char *pText, char *pData, char *pBss,
			 SYMTAB_ID symTbl, UINT16 group);
extern STATUS loadSegmentsAllocate (SEG_INFO *pSeg);

#else   /* __STDC__ */

extern MODULE_ID loadModule ();
extern MODULE_ID loadModuleAt ();
extern MODULE_ID loadModuleAtSym ();
extern MODULE_ID loadModuleGet ();
extern void addSegNames ();
extern STATUS loadSegmentsAllocate ();

#endif  /* __STDC__ */

#ifdef __cplusplus
}
#endif

#endif /* __INCloadLibh */
