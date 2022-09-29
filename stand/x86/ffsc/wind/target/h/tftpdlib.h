/* tftpdLib.h - vxWorks Trival File Transfer protocol header file */

/* Copyright 1992-1992 Wind River Systems, Inc. */

/*
modification history
--------------------
02b,22sep92,rrr  added support for c++
02a,04jul92,jcf  cleaned up.
01b,26may92,rrr  the tree shuffle
		  -changed includes to have absolute path from h/
01a,06Feb92,jmm  written.
*/

#ifndef __INCtftpdLibh
#define __INCtftpdLibh

#ifdef __cplusplus
extern "C" {
#endif

#include "vwmodnum.h"
#include "lstlib.h"
#include "iolib.h"
#include "netinet/in.h"
#include "tftplib.h"

typedef struct tftpd_dir
    {
    NODE	node;
    char	dirName [MAX_FILENAME_LENGTH];
    } TFTPD_DIR;

#if defined(__STDC__) || defined(__cplusplus)

extern STATUS 	tftpdInit (int stackSize, int nDirectories,
			   char ** directoryNames, BOOL noControl,
			   int maxConnections);
extern STATUS 	tftpdTask (int nDirectories, char ** directoryNames,
			   int maxConnections);
extern STATUS 	tftpdDirectoryAdd (char *fileName);
extern STATUS 	tftpdDirectoryRemove (char *fileName);

#else	/* __STDC__ */

extern STATUS 	tftpdInit ();
extern STATUS 	tftpdTask ();
extern STATUS 	tftpdDirectoryAdd ();
extern STATUS 	tftpdDirectoryRemove ();

#endif	/* __STDC__ */

#ifdef __cplusplus
}
#endif

#endif /* __INCtftpdLibh */
