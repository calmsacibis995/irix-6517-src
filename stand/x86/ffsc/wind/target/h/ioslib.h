/* iosLib.h - I/O system header file */

/* Copyright 1984-1992 Wind River Systems, Inc. */

/*
modification history
--------------------
01t,15jan93,wmd  added S_iosLib_INVALID_ETHERNET_ADDRESS
01s,22sep92,rrr  added support for c++
01r,23aug92,jcf  added iosShowInit() prototype.
		 changed to use dllLib.
01q,04jul92,jcf  cleaned up.
01p,26may92,rrr  the tree shuffle
01o,04oct91,rrr  passed through the ansification filter
		  -changed VOID to void
		  -changed copyright notice
01n,05oct90,dnw  deleted private routines.
01m,05oct90,shl  added ANSI function prototypes.
                 made #endif ANSI style.
                 added copyright notice.
01l,10aug90,dnw  added declaration of iosFdFree().
01k,07may90,hjb  added IMPORT declaration of iosFdDevFind().
01j,30jun88,llk  added definition of iosNextDevGet().
01i,04jun88,llk  removed S_iosLib_INVALID_DEVICE_NAME.
01h,12apr88,gae  moved defn's of FD_ENTRY & DRV_ENTRY to iosLib.c.
01g,16dec87,jlf  changed de_dummy field in DRV_ENTRY to de_inuse.
01f,30sep87,gae  added name field to FD_ENTRY.
01e,29apr87,dnw  added S_iosLib_CONTROLLER_NOT_PRESENT.
01d,24dec86,gae  changed stsLib.h to vwModNum.h.
01c,01dec86,dnw  changed DEV_HDR.name to be ptr to name instead array of name.
01b,07aug84,ecs  added status codes and include of stsLib.h
01a,08jun84,dnw  written
*/

#ifndef __INCiosLibh
#define __INCiosLibh

#ifdef __cplusplus
extern "C" {
#endif

#include "iolib.h"
#include "dlllib.h"
#include "vwmodnum.h"

/* status codes */

#define S_iosLib_DEVICE_NOT_FOUND		(M_iosLib | 1)
#define S_iosLib_DRIVER_GLUT			(M_iosLib | 2)
#define S_iosLib_INVALID_FILE_DESCRIPTOR	(M_iosLib | 3)
#define S_iosLib_TOO_MANY_OPEN_FILES		(M_iosLib | 4)
#define S_iosLib_CONTROLLER_NOT_PRESENT		(M_iosLib | 5)
#define S_iosLib_DUPLICATE_DEVICE_NAME		(M_iosLib | 6)
#define S_iosLib_INVALID_ETHERNET_ADDRESS	(M_iosLib | 7)

typedef struct 		/* DEV_HDR - device header for all device structures */
    {
    DL_NODE	node;		/* device linked list node */
    short	drvNum;		/* driver number for this device */
    char *	name;		/* device name */
    } DEV_HDR;


/* function declarations */


#if defined(__STDC__) || defined(__cplusplus)

extern STATUS 	iosInit (int max_drivers, int max_files, char *nullDevName);
extern void 	iosShowInit (void);
extern DEV_HDR *iosDevFind (char *name, char ** pNameTail);
extern DEV_HDR *iosFdDevFind (int fd);
extern DEV_HDR *iosNextDevGet (DEV_HDR *pDev);
extern STATUS 	iosClose (int fd);
extern STATUS 	iosDevAdd (DEV_HDR *pDevHdr, char *name, int drvnum);
extern STATUS 	iosDrvRemove (int drvnum, BOOL forceClose);
extern int 	iosCreate (DEV_HDR *pDevHdr, char *fileName, int mode);
extern int 	iosDelete (DEV_HDR *pDevHdr, char *fileName);
extern int 	iosDrvInstall (FUNCPTR pCreate, FUNCPTR pDelete, FUNCPTR pOpen,
			       FUNCPTR pClose, FUNCPTR pRead, FUNCPTR pWrite,
			       FUNCPTR pIoctl);
extern int 	iosFdNew (DEV_HDR *pDevHdr, char *name, int value);
extern int 	iosFdValue (int fd);
extern int 	iosIoctl (int fd, int function, int arg);
extern int 	iosOpen (DEV_HDR *pDevHdr, char *fileName, int flags, int mode);
extern int 	iosRead (int fd, char *buffer, int maxbytes);
extern int 	iosWrite (int fd, char *buffer, int nbytes);
extern void 	iosDevDelete (DEV_HDR *pDevHdr);
extern void 	iosDevShow (void);
extern void 	iosDrvShow (void);
extern void 	iosFdFree (int fd);
extern void 	iosFdSet (int fd, DEV_HDR *pDevHdr, char *name, int value);
extern void 	iosFdShow (void);

#else	/* __STDC__ */

extern STATUS 	iosInit ();
extern void 	iosShowInit ();
extern DEV_HDR *iosDevFind ();
extern DEV_HDR *iosFdDevFind ();
extern DEV_HDR *iosNextDevGet ();
extern STATUS 	iosClose ();
extern STATUS 	iosDevAdd ();
extern STATUS 	iosDrvRemove ();
extern int 	iosCreate ();
extern int 	iosDelete ();
extern int 	iosDrvInstall ();
extern int 	iosFdNew ();
extern int 	iosFdValue ();
extern int 	iosIoctl ();
extern int 	iosOpen ();
extern int 	iosRead ();
extern int 	iosWrite ();
extern void 	iosDevDelete ();
extern void 	iosDevShow ();
extern void 	iosDrvShow ();
extern void 	iosFdFree ();
extern void 	iosFdSet ();
extern void 	iosFdShow ();

#endif	/* __STDC__ */

#ifdef __cplusplus
}
#endif

#endif /* __INCiosLibh */
