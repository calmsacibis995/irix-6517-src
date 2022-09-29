/* objLib.h - object management library header */

/* Copyright 1984-1992 Wind River Systems, Inc. */

/*
modification history
--------------------
01o,31aug94,rdc  beefed up OBJ_VERIFY.
01n,15oct93,cd   added #ifndef _ASMLANGUAGE.
01m,10dec93,smb  modified OBJ_VERIFY for windview level 1 instrumentation
01l,22sep92,rrr  added support for c++
01k,29jul92,jcf  added errno.h include.
01j,04jul92,jcf  cleaned up.
01i,26may92,rrr  the tree shuffle
01h,04dec91,rrr  some ansi cleanup.
01g,04oct91,rrr  passed through the ansification filter
		  -fixed #else and #endif
		  -changed VOID to void
		  -changed copyright notice
01f,10jun91.del  added pragma for gnu960 alignment.
01e,05apr91,gae  added NOMANUAL to avoid fooling mangen.
01d,05oct90,shl  added ANSI function prototypes.
                 made #endif ANSI style.
		 added copyright notice.
01c,15jul90,dnw  changed objAlloc() from (char*) to (void*)
		 added objAllocExtra()
01b,26jun90,jcf  added objAlloc()/objFree().
		 added standard object status codes.
01a,10dec89,jcf  written.
*/

#ifndef __INCobjLibh
#define __INCobjLibh

#ifdef __cplusplus
extern "C" {
#endif

#include "vxworks.h"
#include "errno.h"
#include "vwmodnum.h"


/* status codes */

#define S_objLib_OBJ_ID_ERROR			(M_objLib | 1)
#define S_objLib_OBJ_UNAVAILABLE		(M_objLib | 2)
#define S_objLib_OBJ_DELETED			(M_objLib | 3)
#define S_objLib_OBJ_TIMEOUT			(M_objLib | 4)
#define S_objLib_OBJ_NO_METHOD			(M_objLib | 5)


#ifndef _ASMLANGUAGE
/* typedefs */

typedef struct obj_core  *OBJ_ID;		/* object id */


/*******************************************************************************
*
* OBJ_VERIFY - check the validity of an object pointer
*
* This macro verifies the existence of the specified object by comparing the
* object's classId with the expected class id.  If the object does not check
* out, errno is set with the appropriate invalid id status.
*
* This macro also checks for the existance of an windview instrumented object.
*
* RETURNS: OK or ERROR if invalid object id
*
* NOMANUAL
*/

#define OBJ_VERIFY(objId,classId)					 \
    (                                                                    \
      (									 \
      (((OBJ_ID)(objId))->pObjClass == (struct obj_class *)(classId)) || \
       (								 \
       ((OBJ_ID)(objId))->pObjClass &&					 \
         (((OBJ_ID)(objId))->pObjClass ==                                \
        (CLASS_ID)(((struct obj_class *)(classId))->initRtn))  		 \
       )								 \
      )                                                                  \
        ?                                                                \
            OK                                                           \
        :                                                                \
            (errno = S_objLib_OBJ_ID_ERROR, ERROR)                       \
    )


/* function declarations */

#if defined(__STDC__) || defined(__cplusplus)

extern STATUS 	objShow (OBJ_ID objId, int showType);

#else	/* __STDC__ */

extern STATUS 	objShow ();

#endif	/* __STDC__ */

#endif /* _ASMLANGUAGE */

#ifdef __cplusplus
}
#endif

#endif /* __INCobjLibh */
