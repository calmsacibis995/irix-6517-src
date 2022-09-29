/* symSync.h - symbol table synchronisation */

/* Copyright 1984-1993 Wind River Systems, Inc. */

/*
modification history
--------------------
01a,06apr93,smb written
*/

#ifndef __INCsymsynch
#define __INCsymsynch


#ifdef __cplusplus
extern "C" {
#endif

/* Function declarations */

#if defined(__STDC__) || defined(__cplusplus)

STATUS symTblLock (SYMTAB_ID symTblId);
STATUS symTblUnlock (SYMTAB_ID symTblId);

#else   /* __STDC__ */

STATUS symTblLock ();
STATUS symTblUnlock ();

#endif  /* __STDC__ */

#ifdef __cplusplus
}
#endif

#endif /* __INCsymsynch*/
