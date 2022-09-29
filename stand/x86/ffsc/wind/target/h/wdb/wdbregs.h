/* wdbRegs.h - header file for register layout as view by the WDB agent */

/* Copyright 1984-1994 Wind River Systems, Inc. */

/*
modification history
--------------------
01b,25may95,ms	defined register objects.
01a,03may95,ms  written.
*/

#ifndef __INCwdbRegsh
#define __INCwdbRegsh

/* includes */

#include "wdb/dll.h"
#include "wdb/wdb.h"
#include "regs.h"		/* WDB_IU_REGS = VxWorks REG_SET */
#include "fpplib.h"		/* WDB_FPU_REGS = VxWorks FPREG_SET */

/* data types */

#if     CPU_FAMILY==MC680X0
typedef struct WDB_IU_REGS
    {
    REG_SET     regSet;
    int         padding;
    } WDB_IU_REGS;
#else
typedef REG_SET WDB_IU_REGS;
#endif

typedef FPREG_SET WDB_FPU_REGS;

typedef struct
    {
    dll_t		node;
    WDB_REG_SET_TYPE	regSetType;		/* type of register set */
    void		(*save) (void);		/* save current regs */
    void		(*load) (void);		/* load saved regs */
    void		(*get)  (char ** ppRegs); /* get saved regs */
    void		(*set)  (char *pRegs);	/* change saved regs */
    } WDB_REG_SET_OBJ;

/* function prototypes */

#ifdef  __STDC__

extern	WDB_REG_SET_OBJ * wdbFpLibInit		(void);
extern	void              wdbExternRegSetObjAdd (WDB_REG_SET_OBJ *pRegSet);

#else	/* ! __STDC__ */

#endif	/* __STDC__ */

extern	WDB_REG_SET_OBJ * wdbFpLibInit		();
extern	void              wdbExternRegSetObjAdd ();

#endif  /* __INCwdbRegsh */

