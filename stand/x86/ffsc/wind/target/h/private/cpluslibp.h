/* private/cplusLibP.h - VxWorks C++ support */

/* Copyright 1992,1993 Wind River Systems, Inc. */

/*
modification history
--------------------
01j,31oct93,srh  added cplusNewHdlMutex
01i,25apr93,srh  added non-ANSI declaration for cplusUnloadFixup...
01h,25apr93,srh  added declaration for cplusUnloadFixup.
01g,25apr93,srh  fixed a typo which was introduced during 01f
01f,24apr93,srh  removed extraneous comma from CPLUS_XTOR_STRATEGIES to
		 please certain compilers.
01e,23apr93,srh  added declarations for cplusLibMinInit
01d,31jan93,srh  renamed from cplusLib.h, coalesced other cplusLib hdrs
01c,22sep92,rrr  added support for c++
01b,03aug92,ajm  corrected non __STDC__ define of cplusDemangle
01a,30jul92,srh  written
*/

#ifndef __INCcplusLibPh
#define __INCcplusLibPh

#include "vxworks.h"
#include "hashlib.h"
#include "limits.h"
#include "modulelib.h"
#include "semlib.h"
#include "symbol.h"
#include "symlib.h"
#include "syssymtbl.h"

/* type declarations */

typedef enum
    {
    OFF		= 0,
    TERSE	= 1,
    COMPLETE	= 2
    } CPLUS_DEMANGLER_MODES;

typedef enum 
    {
    MANUAL	= 0,
    AUTOMATIC	= 1
    } CPLUS_XTOR_STRATEGIES;

/* C++-only declarations */

#ifdef __cplusplus

/* type declarations */

class RBString_T
    {
public:
    		  RBString_T ();	     // create an empty s
    		  RBString_T (RBString_T &); // create a copy of another
					     // RBString_T
    		  RBString_T (const char *); // create an RBString_T from
  					     // a C string
    void	  clear ();		     // reinitialize this RBString_T
    char	* extractCString (char *, int); // extract C string

    RBString_T	& operator =  (RBString_T &);
  
    BOOL 	  operator == (RBString_T &) const;
    BOOL 	  operator != (RBString_T &) const;

    int		  length () const	  // return number of characters,
		    { return nChars; }	  // not counting NUL.

    RBString_T	& append (RBString_T &);  // append RBString_T contents
    RBString_T	& append (const char *,	  // append C string contents
			  int len = INT_MAX);
    RBString_T  & append (char);	  // append character

    RBString_T	& prepend (RBString_T &); // prepend RBString_T contents
    RBString_T	& prepend (const char *,  // prepend C string contents
			   int len = INT_MAX);
    RBString_T	& prepend (char);	  // prepend character

protected:
    char	  appendChar (char);      // append a character
    char	  prependChar (char);	  // prepend a character
  
protected:
    char	  data [ MAX_SYS_SYM_LEN ]; // data
    char	* head;			    // pointer to first character
					    // in string
    char	* tail;			    // pointer to delimiting
					    // NUL character
    int		  nChars;		    // number of characters currently
					    // in string, not counting NUL

private:
    friend class RBStringIterator_T;
    };

class RBStringIterator_T
    {
public:
    		RBStringIterator_T (const RBString_T &); // initialize a
  							 // new iterator
    char	nextChar ();				 // fetch next char.

protected:
    const RBString_T	* theRBString;
    const char 		* nc;
    };

struct ArrayDesc_T
    {
    // functions
    ArrayDesc_T (void *pObject, int nElems);

    // data
    HASH_NODE	  hashNode;
    void 	* pObject;
    int		  nElems;
    };

class ArrayStore_T
    {
 public:
    	   	  ArrayStore_T ();
		 ~ArrayStore_T ();
    void 	  insert (void * pObject, int nElems);
    int  	  fetch  (void *pObject);
 private:
    HASH_ID	  hashId;
    SEM_ID	  mutexSem;
    };

class Demangle_T
    {
 public:
    Demangle_T (const char *);

    char	* extractDemangled (char * buf, int length = MAX_SYS_SYM_LEN);
    BOOL	  isDemangled ();
  
 private:
    BOOL	  scanArg (RBString_T * shadow = 0, RBString_T * retType = 0);
    BOOL	  scanArgs (RBString_T * shadow = 0, RBString_T * retType = 0);
    BOOL	  scanCastOperator ();
    BOOL	  scanClassName (RBString_T * shadow = 0);
    BOOL	  scanCtorDtor ();
    BOOL	  scanFunctionName ();
    BOOL	  scanNName (RBString_T * shadow = 0);
    BOOL	  scanSimpleFunctionName ();
    BOOL	  scanSpecialFunctionName ();
    BOOL          scanSimpleOperator ();
    BOOL	  demangle (const char *mangledName);

    Demangle_T ()					{}
    Demangle_T (const Demangle_T &)			{}
    void	  operator = (const Demangle_T &)	{}

    const char	* theMangledName;
    const char	* currentPosition;
    RBString_T	  workingString;
    BOOL	  demanglingSucceeded;

    enum DeclMod_T
	{
	dm_unsigned	= 0x0001,
	dm_const	= 0x0002,
	dm_volatile	= 0x0004,
	dm_signed	= 0x0008,
	dm_pointer	= 0x0010,
	dm_reference	= 0x0020,
	dm_array	= 0x0040,
	dm_function	= 0x0080,
	dm_ptrToMember  = 0x0100,
	};

    class Declaration_T
        {
    public:
	Declaration_T ();
	Declaration_T (const Declaration_T &);
	Declaration_T	& operator =  (const Declaration_T &);
	Declaration_T	& operator += (const DeclMod_T);
        Declaration_T	& operator -= (const DeclMod_T);
			  
	BOOL		  is (const DeclMod_T) const;
			  
    private:
	long		  contents;
	};
    };

/* inline definitions */

// cplusLogMsg
//
typedef	int	 	(* FUNCPTR_ARGS) (...);
extern 	FUNCPTR_ARGS	   _func_logMsg;
inline void cplusLogMsg (char *fmt, int arg1, int arg2, int arg3,
			 int arg4, int arg5, int arg6)
    {
    if (_func_logMsg != 0)
	{
	(* _func_logMsg) (fmt, arg1, arg2, arg3, arg4, arg5, arg6);
	}
    }

// ArrayDesc_T :: ArrayDesc_T
//
inline ArrayDesc_T :: ArrayDesc_T (void *pObject, int nElems)
    {
    this->pObject = pObject;
    this->nElems = nElems;
    }

#endif				/* __cplusplus (C++-only declarations) */

/* data declarations */

extern CPLUS_DEMANGLER_MODES cplusDemanglerMode;
extern CPLUS_XTOR_STRATEGIES cplusXtorStrategy;
extern SEM_ID                cplusNewHdlMutex;

/* function declarations, C linkage */

#ifdef __cplusplus
extern "C" {
#endif

extern void cplusCallCtors (VOIDFUNCPTR *ctors);
extern void cplusCallDtors (VOIDFUNCPTR *dtors);

#if defined(__STDC__) || defined(__cplusplus)

extern void	cplusArraysInit ();
extern BOOL	cplusMatchMangled (SYMTAB_ID symTab, char *string,
				   SYM_TYPE *pType, int *pValue);
extern char *	cplusDemangle (char *source, char *dest, int n);
extern STATUS	cplusLibInit (void);
extern STATUS	cplusLibMinInit (void);
extern STATUS	cplusLoadFixup (MODULE_ID module, int symFlag,
				SYMTAB_ID symTab);
extern STATUS   cplusUnloadFixup (MODULE_ID module);

#else   /* __STDC__ */

extern void	cplusArraysInit ();
extern BOOL	cplusMatchMangled ();
extern char *	cplusDemangle ();
extern STATUS	cplusLibInit ();
extern STATUS	cplusLibMinInit ();
extern STATUS	cplusLoadFixup ();
extern STATUS   cplusUnloadFixup ();

#endif  /* __STDC__ */

#ifdef __cplusplus
}
#endif

#endif /* __INCcplusLibPh */
