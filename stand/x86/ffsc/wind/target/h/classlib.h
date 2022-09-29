/* classLib.h - object class management library header file */

/* Copyright 1984-1992 Wind River Systems, Inc. */

/*
modification history
--------------------
01h,15oct93,cd   added #ifndef _ASMLANGUAGE.
01g,22sep92,rrr  added support for c++
01f,04jul92,jcf  cleaned up.
01e,26may92,rrr  the tree shuffle
01d,04oct91,rrr  passed through the ansification filter
		  -fixed #else and #endif
		  -changed copyright notice
01c,05oct90,shl  added ANSI function prototypes.
                 made #endif ANSI style.
		 added copyright notice.
01b,26jun90,jcf  added object allocation/termination/etc. count
01a,10dec89,jcf  written.
*/

#ifndef __INCclassLibh
#define __INCclassLibh

#ifdef __cplusplus
extern "C" {
#endif

#include "vxworks.h"
#include "vwmodnum.h"
#include "objlib.h"
#include "memlib.h"

/* status codes */

#define S_classLib_CLASS_ID_ERROR		(M_classLib | 1)
#define S_classLib_NO_CLASS_DESTROY		(M_classLib | 2)

#ifndef _ASMLANGUAGE

typedef struct obj_class *CLASS_ID;		/* CLASS_ID */

extern CLASS_ID classClassId;			/* class object class id */

/* function declarations */

#if defined(__STDC__) || defined(__cplusplus)

extern STATUS 	classLibInit (void);
extern STATUS 	classMemPartIdSet (CLASS_ID classId, PART_ID memPartId);
extern void 	classShowInit (void);
extern STATUS 	classShow (CLASS_ID classId, int level);

#else	/* __STDC__ */

extern STATUS 	classLibInit ();
extern STATUS 	classMemPartIdSet ();
extern void 	classShowInit ();
extern STATUS 	classShow ();

#endif	/* __STDC__ */

#endif /* _ASMLANGUAGE */

#ifdef __cplusplus
}
#endif

#endif /* __INCclassLibh */
