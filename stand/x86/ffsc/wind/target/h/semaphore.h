/* semaphore.h - header for POSIX 1003.4 synchronization */

/* Copyright 1984-1994 Wind River Systems, Inc. */

/*
modification history
--------------------
01c,08apr94,dvs  added pragma for gnu960 alignment.
01b,05jan94,kdl	 changed sem_t "close" field to "refCnt"; general cleanup.
01a,06apr93,smb	 written
*/

#ifndef __INCsemaphoreh
#define __INCsemaphoreh

#ifdef __cplusplus
extern "C" {
#endif

#include "limits.h"
#include "semlib.h"
#include "objlib.h"
#include "symlib.h"

#define SEM_VALUE_MAX 100 	/* maximum value to initialize a semaphore */
#define SEM_NSEMS_MAX 100 	/* maximum number of semaphores in the system */

extern SYMTAB_ID       posixNameTbl;              /* POSIX symbol table id */
 
#if ((CPU_FAMILY==I960) && (defined __GNUC__))
#pragma align 1                 /* tell gcc960 not to optimize alignments */
#endif  /* CPU_FAMILY==I960 */

typedef struct sem_des 		/* sem_t */
    {
    OBJ_CORE    objCore;        /* semaphore object core */
    SEM_ID      semId;		/* semaphore identifier */
    int         refCnt;		/* number of attachments */
    char *      sem_name; 	/* name of semaphore */
    } sem_t;

#if ((CPU_FAMILY==I960) && (defined __GNUC__))
#pragma align 0                 /* turn off alignment requirement */
#endif  /* CPU_FAMILY==I960 */

#undef _POSIX_NO_TRUNC		/* do not limit pathname length */

/* Function declarations */

#if defined(__STDC__) || defined(__cplusplus)

extern int 	sem_init ( sem_t *, int, unsigned int);
extern int 	sem_destroy ( sem_t *);
extern sem_t * 	sem_open ( const char *, int, ...);
extern int 	sem_close ( sem_t *);
extern int 	sem_unlink ( const char *);
extern int 	sem_wait ( sem_t *);
extern int 	sem_trywait ( sem_t *);
extern int 	sem_post ( sem_t *);
extern int 	sem_getvalue ( sem_t *, int *);

#else   /* __STDC__ */

extern int 	sem_init ();
extern int 	sem_destroy ();
extern sem_t * 	sem_open ();
extern int 	sem_close ();
extern int 	sem_unlink ();
extern int 	sem_wait ();
extern int 	sem_trywait ();
extern int 	sem_post ();
extern int 	sem_getvalue ();

#endif  /* __STDC__ */

#ifdef __cplusplus
}
#endif

#endif /* __INCsemaphoreh */
