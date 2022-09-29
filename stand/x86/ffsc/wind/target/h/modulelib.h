/* moduleLib.h - symbol structure header */

/* Copyright 1992 Wind River Systems, Inc. */

/*
modification history
--------------------
01e,30oct92,jmm  added prototype for moduleCheck() - spr 1693
01d,22sep92,rrr  added support for c++
01c,27aug92,jmm  changed MODULE_SEG_INFO struct to always include TEXT/DATA/BSS
                 changed symFlags to flags
		 added prototypes for moduleNameGet(), moduleFlagsGet()
		 changed type for ctors and dtors to VOIDFUNCPTR
01b,01aug92,srh  added lists for static constructors/destructors to
		 MODULE struct.
01a,09apr92,jmm	 written
*/

#ifndef __INCmoduleLibh
#define __INCmoduleLibh

#ifdef __cplusplus
extern "C" {
#endif

#include "dlllib.h"
#include "slllib.h"
#include "limits.h"
#include "objlib.h"
#include "private/objlibp.h"

/* moduleLib Status Codes */

#define S_moduleLib_MODULE_NOT_FOUND             (M_moduleLib | 1)
#define S_moduleLib_HOOK_NOT_FOUND               (M_moduleLib | 2)
#define S_moduleLib_BAD_CHECKSUM                 (M_moduleLib | 3)

/* Types of object module formats */

#define MODULE_A_OUT 0		/* format is a.out */
#define MODULE_B_OUT 1		/* format is b.out */
#define MODULE_ECOFF 2		/* format is ECOFF */
#define MODULE_ELF   3		/* format is ELF */

/* Types of segments */

/* Note - These are specific to a.out */

#define SEGMENT_TEXT	2	/* text segment */
#define SEGMENT_DATA	4	/* data segment */
#define SEGMENT_BSS	8	/* bss segment */

/* These are needed for the HP-PA SOM COFF */

#define SEGMENT_UNWIND  0x10000
#define SEGMENT_STUB    0x20000

/* segment flag bits */

#define SEG_FREE_MEMORY		1 /* Free memory when deleting this segment */
#define SEG_WRITE_PROTECTION	2 /* Segment's memory is write-protected,
				   * need to call memory managment routines
				   * to unprotect when deleting */

/* display options */

#define MODDISPLAY_CODESIZE  	1 /* Display size of the code segments */
#define MODDISPLAY_IS_DLL_NODE 	2 /* Display routine is being called with
				   * DLL_NODE instead of MODULE_ID -
				   * used by moduleShow() */

#define MODDISPLAY_ALL  	(MODDISPLAY_CODESIZE) /* Display all possible
						       * information */

/* moduleCheck() options */

#define MODCHECK_NOPRINT	1 /* Don't print module names during check */

#define MODCHECK_TEXT		(SEGMENT_TEXT) /* check text segment */
#define MODCHECK_DATA		(SEGMENT_DATA) /* check data segment */
#define MODCHECK_BSS	       	(SEGMENT_BSS)  /* check bss segment */

/* module status information */

#define MODULE_REPLACED	0x00010000 /* set if the module is loaded twice */

typedef struct
    {
    DL_NODE	segmentNode;	/* double-linked list node information*/
    void *	address;	/* segment address */
    int		size;		/* segment size */
    int		type;		/* segment type*/
    int		flags;		/* segment flags */
    u_short	checksum;	/* segment checksum; u_short is returned by
				 * checksum() */
    } SEGMENT;

typedef SEGMENT *SEGMENT_ID;

typedef struct
    {
    OBJ_CORE	objCore;	/* object management */
    DL_NODE	moduleNode;	/* double-linked list node information */
    char	name [NAME_MAX]; /* module name */
    char	path [PATH_MAX]; /* module path */
    int		flags;		/* symFlags as passed to the loader */
    DL_LIST	segmentList;	/* list of segments */
    int		format;		/* object module format */
    UINT16	group;		/* group number */
    DL_LIST	dependencies;	/* list of modules that this depends on */
    DL_LIST	referents;	/* list of modules that refer to this */
    VOIDFUNCPTR	* ctors;	/* list of static constructor callsets */
    VOIDFUNCPTR * dtors;	/* list of static destructor callsets */
    void *	user1;		/* reserved for use by end-user */
    void *	reserved1;	/* reserved for use by WRS */
    void *	reserved2;	/* reserved for use by WRS */
    void *	reserved3;	/* reserved for use by WRS */
    void *	reserved4;	/* reserved for use by WRS */
    } MODULE;

typedef MODULE *MODULE_ID;

typedef struct
    {
    void *	textAddr;	/* address of text segment */
    void *	dataAddr;	/* address of data segment */
    void *	bssAddr;	/* address of bss segment */
    int 	textSize;	/* size of text segment */
    int 	dataSize;	/* size of data segment */
    int 	bssSize;	/* size of bss segment */
    } MODULE_SEG_INFO;

typedef struct
    {
    char        	name [NAME_MAX]; /* module name */
    int         	format;          /* object module format */
    int			group;		 /* group number */
    MODULE_SEG_INFO	segInfo; 	 /* segment info */
    } MODULE_INFO;

/* function declarations */

#if defined(__STDC__) || defined(__cplusplus)

extern STATUS moduleLibInit (void);
extern MODULE_ID moduleCreate (char *name, int format, int flags);
extern STATUS moduleInit (MODULE_ID moduleId, char *name, int format,
			  int flags);
extern STATUS moduleTerminate (MODULE_ID moduleId);
extern STATUS moduleDelete (MODULE_ID moduleId);
extern MODULE_ID moduleIdFigure (void *moduleNameOrId);
extern STATUS moduleShow (char *moduleNameOrId, int options);
extern STATUS moduleSegAdd (MODULE_ID moduleId, int type, void *location,
			    int length, int flags);
extern SEGMENT_ID moduleSegGet (MODULE_ID moduleId);
extern SEGMENT_ID moduleSegFirst (MODULE_ID moduleId);
extern SEGMENT_ID moduleSegNext (SEGMENT_ID segmentId);
extern SEGMENT_ID moduleSegEach (MODULE_ID moduleId, FUNCPTR routine,
				 int userArg);
extern STATUS moduleCreateHookAdd (FUNCPTR moduleCreateHookRtn);
extern STATUS moduleCreateHookDelete (FUNCPTR moduleCreateHookRtn);
extern MODULE_ID moduleFindByName (char *moduleName);
extern MODULE_ID moduleFindByNameAndPath (char *moduleName, char *pathName);
extern MODULE_ID moduleFindByGroup (int groupNumber);
extern MODULE_ID moduleEach (FUNCPTR routine, int userArg);
extern int moduleIdListGet (MODULE_ID *idList, int maxModules);
extern STATUS moduleInfoGet (MODULE_ID moduleId, MODULE_INFO *pModuleInfo);
extern char * moduleNameGet (MODULE_ID moduleId);
extern int moduleFlagsGet (MODULE_ID moduleId);
STATUS moduleCheck (int options);
 
#else   /* __STDC__ */

extern STATUS moduleLibInit ();
extern MODULE_ID moduleCreate ();
extern STATUS moduleInit ();
extern STATUS moduleTerminate ();
extern STATUS moduleDelete ();
extern MODULE_ID moduleIdFigure ();
extern STATUS moduleShow ();
extern STATUS moduleSegAdd ();
extern SEGMENT_ID moduleSegGet ();
extern SEGMENT_ID moduleSegFirst ();
extern SEGMENT_ID moduleSegNext ();
extern SEGMENT_ID moduleSegEach ();
extern STATUS moduleCreateHookAdd ();
extern STATUS moduleCreateHookDelete ();
extern MODULE_ID moduleFindByName ();
extern MODULE_ID moduleFindByNameAndPath ();
extern MODULE_ID moduleFindByGroup ();
extern MODULE_ID moduleEach ();
extern int moduleIdListGet ();
extern STATUS moduleInfoGet ();
STATUS moduleCheck ();

#endif  /* __STDC__ */

#ifdef __cplusplus
}
#endif

#endif /* __INCmoduleLibh */
