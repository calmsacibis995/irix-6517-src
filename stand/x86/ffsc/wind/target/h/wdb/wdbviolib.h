/* wdbVioLib.h - VIO header file for remote debug server */

/* Copyright 1984-1994 Wind River Systems, Inc. */

/*
modification history
--------------------
01c,24may95,ms  added wdbVioChannelUnregister().
01b,05apr95,ms  new data types.
01a,15nov94,ms  written.
*/

#ifndef __INCwdbVioLibh
#define __INCwdbVioLibh

/* includes */

#include "wdb/wdb.h"
#include "wdb/dll.h"

/* data types */

typedef struct wdb_vio_node
    {
    dll_t	node;
    uint_t      channel;
    int         (*inputRtn) (struct wdb_vio_node *pNode, char *pData,
                             uint_t nBytes);
    void *      pVioDev;
    } WDB_VIO_NODE;

/* function prototypes */

#if defined(__STDC__)

extern void	wdbVioLibInit	  	(void);
extern STATUS	wdbVioChannelRegister	(WDB_VIO_NODE *pVioNode);
extern void	wdbVioChannelUnregister (WDB_VIO_NODE *pVioNode);

#else   /* __STDC__ */

extern void	wdbVioLibInit	 	();
extern STATUS	wdbVioDevRegister	();
extern void	wdbVioChannelUnregister	();

#endif  /* __STDC__ */

#endif /* __INCwdbVioLibh */

