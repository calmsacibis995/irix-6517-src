#ifndef DLOAD_H
#define DLOAD_H
/*
 * dload.h - environment and error codes for dynamic loader.
 *
 * Copyright 1991, Silicon Graphics, Inc.
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Silicon Graphics, Inc.
 *
 * RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
 * and Computer Software clause at DFARS 252.227-7013, and/or in similar or
 * successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -
 * rights reserved under the Copyright Laws of the United States.
 */

/* ********************************************************************** *\
 *         Copyright IBM Corporation 1988,1989 - All Rights Reserved      *
 *        For full copyright information see:'andrew/config/COPYRITE'     *
\* ********************************************************************** */
/* 
	doload.h - environment for dynamic loader

	Author:  Zalman Stern July 1989
 */

/********************************************************************
 *
 *   DLload.h - environment and error codes for dynamic loader
 *  
 *   Jay L. Gischer, Silicon Graphics Inc. July 1991
 *   
 ********************************************************************/

/* INCLUDES *******************************************************/

/*
 * format of bootable a.out file headers
 */
struct execinfo {
	struct filehdr fh;
	struct aouthdr ah;
	struct scnhdr *sh;
};

/* here is the state during a load */

struct doload_environment {
    int fd;			/* input file descriptor */
    int problems;		/* count of problems encountered */
    int status;                 /* place to report error type */
    jmp_buf errorJump;		/* error exit */
    void *text;			/* text segment */
    void *data;                 /* data, rdata, bss segments */ 
	void (*initpoint)() ;	/* start of the ".init" section */
	char *gpBase ;		/* start of the "near" data area */
	int gpSize ;		/* size of the "near" data area */
	int gpOffset ;		/* Change in "near" data area */
	int firstGpSection ;	/* usually .lit8 */
	char *membase ;		/* initial page of mapped file */
	char *commonArea ;	/* malloc'ed common symbols */
	int runTimeGnum ;	/* equivolent to ld -G xxx */
    long totalsize;
    struct filehdr *filehdr;	/* header at beginning of a.out file */
    struct aouthdr *aouthdr;	/* Where the entry point resides among
				 * other things. */
    HDRR symheader;
    struct scnhdr *sections;
    void **contents ;	/* Allocated with "sections" DON'T FREE */
    struct reloc *rtab;		/* relocation table */
    pEXTR symtab;		/* symbol table */
    char *stringtab;		/* string table */
} ;

#define doload_extension ".do"

extern void (* DLload( int, void *, void *, void *, int * ) )() ;

#define DLerr_None 0         /* Status is normal, no problems */
#define DLerr_REFLOSplit 1   /* Two REFLOS off same REFHI split over 
                                a 64K boundary. */
#define DLerr_NoText 2       /* No .text section present */
#define DLerr_HasInit 3      /* Unexpected .init section present */
#define DLerr_HasGP 4        /* Unexpected gp sections present */
#define DLerr_HasLib 5       /* Unexpected .lib section present */
#define DLerr_BadSeek 6      /* An lseek failure. */
#define DLerr_Prob 7         /* This shouldn't happen */
#define DLerr_RogueScn 8     /* Bad section, should have been caught */
#define DLerr_OutOfMem 9     /* Unsuccessful malloc */
#define DLerr_BadRead 10     /* Read failure. */
#define DLerr_BadJUMPADDR 11 /* Relocation of a JUMPADDR overflows */
#define DLerr_REFLODangle 12 /* REFLO to symbol with different index
			      * Than last REFHI.
			      */
#define DLerr_REFLOWidow 13  /* REFLO not preceded by REFHI. */
#define DLerr_BadGpReloc 14  /* GPREL reloc of external doesn't work. */
#define DLerr_HasGpReloc 15  /* Code has unexpected gp-relative relocs */
#define DLerr_CFFailed 16    /* Cache flush failed. */
#define DLerr_BadExtSC 17    /* Unexpected storage class encountered */
#define DLerr_UndefinedSym 18 /* Undefined external symbol */

/* prototypes */
void (*dload(int, void *, void *, struct execinfo *, int *))();

#endif /* DLOAD_H */
