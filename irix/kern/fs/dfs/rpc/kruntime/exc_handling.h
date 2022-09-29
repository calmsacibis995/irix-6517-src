/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1996 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE in the
 * src directory for the full copyright text.
 */
/*
 * HISTORY
 * $Log: exc_handling.h,v $
 * Revision 65.6  1998/02/26 23:41:40  lmc
 * Removed  REFERENCED additions which were mistakenly integrated.
 * The REFERENCED only seems to work if it is on a line by itself
 * immediately preceding the declaration of a variable.
 *
 * Revision 65.2  1997/10/20  19:16:25  jdoak
 * Initial IRIX 6.4 code merge
 *
 * Revision 1.1.70.2  1996/02/18  23:46:38  marty
 * 	Update OSF copyright years
 * 	[1996/02/18  22:45:19  marty]
 *
 * Revision 1.1.70.1  1995/12/08  00:15:06  root
 * 	Submit OSF/DCE 1.2.1
 * 	[1995/12/07  23:56:19  root]
 * 
 * 	add exc_get_status() macro definition
 * 
 * Revision 1.1.68.2  1994/08/09  17:32:32  burati
 * 	DFS/EPAC/KRPC/dfsbind changes
 * 	[1994/08/09  17:04:05  burati]
 * 
 * Revision 1.1.68.1  1994/01/21  22:32:02  cbrooks
 * 	RPC Code Cleanup - Initial Submission
 * 	[1994/01/21  20:57:23  cbrooks]
 * 
 * Revision 1.1.5.3  1993/01/03  22:36:20  bbelch
 * 	Embedding copyright notice
 * 	[1993/01/03  19:52:34  bbelch]
 * 
 * Revision 1.1.5.2  1992/12/23  19:39:08  zeliff
 * 	Embedding copyright notice
 * 	[1992/12/23  14:53:17  zeliff]
 * 
 * Revision 1.1.3.3  1992/05/28  17:51:48  garyf
 * 	fixes for OSF/1 compilation warnings
 * 	fix incorrect AIX dce_exc_{set,long}jmp defns
 * 
 * Revision 1.1.3.2  1992/05/20  18:31:15  garyf
 * 	cleanup to use correct __OSF__ for osf compilation
 * 	[1992/05/20  18:24:46  garyf]
 * 
 * Revision 1.1  1992/01/19  03:16:13  devrcs
 * 	Initial revision
 * 
 * $EndLog$
 */
#ifndef _EXC_HANDLING_H
#define _EXC_HANDLING_H	 1
/*
**  Copyright (c) 1989 by
**      Hewlett-Packard Company, Palo Alto, Ca. & 
**
**
**  NAME:
**
**      exc_handling.h
**
n**  FACILITY:
**
**      DCE Exception Package
**
**  ABSTRACT:
**
**  An implementation of the various DCE EXCEPTION data types, functions,
**  etc that the runtime and stubs and RPC users use.
**
**
*/

#include <dce/dce.h>

#include <sys/types.h>
#ifdef	SGIMIPS
#include <sys/pcb.h>
#endif

/*
 * Different systems do different things for setjmp...
 * We could split this file up into system dependent / independent
 * portions, but that's getting pretty tiresome and wouldn't work
 * all that well when "applications" are just including 
 * <dce/exc_handling.h>.
 */
#if defined(__OSF__) && !defined(__hp_osf) && !defined(hp_osf)
    /* use the dce_exc_{set,long}jmp() from dce_exc_context_*.s */
#else
#if defined(_AIX)
#  define dce_exc_setjmp(jb)    setjmpx(jb)
#  define dce_exc_longjmp(jb,v) longjmpx(v)
#else
    /* systems with {set,long}jmp() can use this default */
#  define dce_exc_setjmp(jb)    setjmp(jb)
#  define dce_exc_longjmp(jb,v) longjmp(jb,v)
#endif /* _AIX */
#endif /* __OSF__ */

/*
 * In the kernel, there seems to be no good reason to keep exceptions
 * as addresses (since status's are much clearer) and we could make an
 * exception a simple int.  Unfortunately, there are a couple of (probably
 * incorrect) places where the runtime / libnidl seems to know the
 * structure of an exception :-(  so we keep this around for now.
 *
 * Exceptions are initialized to a status value, the mapping being
 * provided by pthread_exc_ids.h.
 */
typedef enum _EXC_KIND {
    _exc_kind_address   = 0x02130455,  
    _exc_kind_status    = 0x02130456
    } _exc_kind;

typedef int _exc_int_t;
typedef char *_exc_address_t;

typedef struct CMA_T_EXCEPTION  {
    _exc_kind  kind;            /* Kind of exception */
    union _EXC_MATCH {  /* The match value */
        _exc_int_t      value;          
        _exc_address_t  address;        
        }   match;
    } EXCEPTION;

#define EXCEPTION_INIT(e)   (   \
        (e).match.value = DCE_CONCAT(e, _id), \
        (e).kind = _exc_kind_status)

/*
 * Define "routine" to equivalence an exception to an integer
 * (typically a system-defined status value).
 */
#define exc_set_status(e,s) ( \
        (e)->match.value = (s), \
        (e)->kind = _exc_kind_status)

/*
 * Define "routine" to return the status of an exception. Returns 0 if status
 * kind (and value of status in *s), or -1 if not status kind.
 */
#define exc_get_status(e,s) ( \
    (e)->kind == _exc_kind_status ? \
        (*(s) = (e)->match.value, 0) : \
        -1)

#define exc_matches(e1,e2) \
  ((e1)->kind == (e2)->kind && (e1)->match.address == (e2)->match.address)

/*
 * The structure of the cleanup handler mechanism is there is a per-thread
 * pointer to the current (innermost) exc_ctxt.  This is implemented
 * as a pointer to a pointer in order to avoid having to do setspecific's
 * when 'pushing' and 'poping'.  The per thread pointer will not exist
 * when the outermost handler is initially invoked and we must be certain
 * to free the storage when the outermost handler is torn down since
 * we don't get a chance to free the storage before the thread process
 * exits.
 *
 * We use the stack to hold the exc_ctxt for each cleanup handler in
 * order to avoid having to heap alloc these bufs all the time.  Arguably,
 * this is not such a smart idea given the static and limited size of
 * kernel stacks and is easily.  The good news is that it is easily
 * corrected.  Additionally, half the time we may already be allocing
 * the per thread storage and could just alloc a little more (i.e. the
 * cost would be lessened).
 *
 * Note that this cleanup mechanism is not integrated with any
 * standard kernel handling (e.g. UNIX kernel u.u_qsave); this
 * is a feature :-)
 */

typedef struct {
    volatile EXCEPTION       exc;        /* the exception causing the unwind to this context */
#ifdef SGIMIPS
    label_t  jmpbuf;
#else
    struct label_t  jmpbuf;
#endif
} pthread_exc_ctxt_t;

#if defined (_AIX)
#define TRY \
      { \
        volatile pthread_exc_ctxt_t exc_ctxt; \
        volatile int exc_handled = false; \
        volatile int cnt = 0; \
        \
        exc_ctxt.exc.kind = _exc_kind_status; \
        exc_ctxt.exc.match.value = setjmpx(&exc_ctxt.jmpbuf); \
        if (exc_ctxt.exc.match.value == 0) {

#define CATCH(e) \
        } else if (exc_matches(&exc_ctxt.exc, &(e))) { \
            EXCEPTION *THIS_CATCH = &exc_ctxt.exc; \
            exc_handled = true;

#define CATCH_ALL \
      } else { \
             EXCEPTION *THIS_CATCH = &exc_ctxt.exc; \
             exc_handled = true;


#define FINALLY \
        } \
        { \
          { if (exc_ctxt.exc.match.value == 0) \
            { \
              clrjmpx(&exc_ctxt.jmpbuf); \
              cnt = 1; \
            } \
          }


#define ENDTRY \
        } \
        if ((exc_ctxt.exc.match.value == 0) && (cnt != 1)) \
        { \
          clrjmpx(&exc_ctxt.jmpbuf); \
        } \
        else if ((!exc_handled) && (cnt != 1)) \
            RAISE(exc_ctxt.exc); \
     }

#define RAISE(rc) longjmpx((rc).match.value)

#define RERAISE   longjmpx((THIS_CATCH)->match.value)

#else
#ifdef	SGIMIPS
#define TRY \
    {   volatile pthread_exc_ctxt_t __exc_ctxt; \
        pthread_exc_ctxt_t *__prev_exc_ctxt; \
        volatile pthread_exc_ctxt_t **__exc_ctxt_head; \
        volatile int __exc_occured; \
        volatile int __exc_handled = false; \
        \
        pthread_exc_setup(&__exc_ctxt_head, &__prev_exc_ctxt); \
        \
        *__exc_ctxt_head = &__exc_ctxt; \
        __exc_occured = dce_exc_setjmp((k_machreg_t *)__exc_ctxt.jmpbuf); \
        if (__exc_occured) \
            *__exc_ctxt_head = __prev_exc_ctxt; \
        if (__exc_occured == 0) { 
            /* user try code... */
#else
#define TRY \
    {   volatile pthread_exc_ctxt_t __exc_ctxt; \
        pthread_exc_ctxt_t *__prev_exc_ctxt; \
        volatile pthread_exc_ctxt_t **__exc_ctxt_head; \
        volatile int __exc_occured; \
        volatile int __exc_handled = false; \
        \
        pthread_exc_setup(&__exc_ctxt_head, &__prev_exc_ctxt); \
        \
        *__exc_ctxt_head = &__exc_ctxt; \
        __exc_occured = dce_exc_setjmp(&__exc_ctxt.jmpbuf); \
        if (__exc_occured) \
            *__exc_ctxt_head = __prev_exc_ctxt; \
        if (__exc_occured == 0) { 
            /* user try code... */
#endif


#define CATCH(e) \
        } else if (exc_matches(&__exc_ctxt.exc, &(e))) { \
            EXCEPTION *THIS_CATCH = (EXCEPTION *)&__exc_ctxt.exc; \
            __exc_handled = true; 
            /* user catch(e) code ... */

#define CATCH_ALL \
        } else { \
            EXCEPTION *THIS_CATCH = (EXCEPTION *)&__exc_ctxt.exc; \
            __exc_handled = true;
            /* user catch_all code ... */

#define FINALLY \
        } \
        { \
            { if (!__exc_occured) \
                *__exc_ctxt_head = __prev_exc_ctxt; \
            }
            /* user finally code ... */

#define ENDTRY \
        } \
        if (!__exc_occured) \
            *__exc_ctxt_head = __prev_exc_ctxt; \
        else if (!__exc_handled) \
            RAISE(__exc_ctxt.exc); \
        \
        if (__prev_exc_ctxt == NULL) \
            pthread_exc_destroy(__exc_ctxt_head); \
    }

#define RAISE(e) pthread_exc_raise(&e)

#define RERAISE  pthread_exc_raise(THIS_CATCH)

void
pthread_exc_setup  _DCE_PROTOTYPE_ ((
    /* [out] */ volatile pthread_exc_ctxt_t *** /* exc_ctxt_head */,
    /* [out] */ pthread_exc_ctxt_t ** /* prev_exc_ctxt */
    ));

void
pthread_exc_destroy  _DCE_PROTOTYPE_ ((
    /* [in] */  volatile pthread_exc_ctxt_t ** /* exc_ctxt_head */
    ));

void
pthread_exc_raise  _DCE_PROTOTYPE_ ((
     /* [in] */   volatile EXCEPTION * /* e */));

#endif /* ! _AIX  */
/* 
 * Exception package defined exceptions (initialized in pthread.c).
 */
extern EXCEPTION exc_e_alerted;

#define cma_e_alerted       exc_e_alerted
#define pthread_cancel_e    exc_e_alerted

#endif /* EXC_H */

