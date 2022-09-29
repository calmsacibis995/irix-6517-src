/* sysSymTbl.h - system symbol table header */

/* Copyright 1984-1992 Wind River Systems, Inc. */

/*
modification history
--------------------
02d,22sep92,rrr  added support for c++
02c,15sep92,jcf  added symbol table declarations.
02b,01aug92,jcf  bumped maximum symbol length to 256.
02a,04jul92,jcf  cleaned up.
01e,26may92,rrr  the tree shuffle
01d,04oct91,rrr  passed through the ansification filter
		  -changed copyright notice
01c,05oct90,shl  added copyright notice.
02b,21nov89,jcf  removed obsolete definition.
02a,08apr86,dnw  removed obsolete definitions.
		 clean-up.
01d,27aug85,rdc  made MAX_SYS_SYM_LEN real big.
01c,24jun85,rdc  changed name in SYS_SYM_STRUCT to char pointer
01b,13jun85,rdc  created MAX_SYS_SYM_LEN
01a,17may85,rdc  written
*/

#ifndef __INCsysSymTblh
#define __INCsysSymTblh

#ifdef __cplusplus
extern "C" {
#endif

#include "symlib.h"
#include "symbol.h"

#define MAX_SYS_SYM_LEN 256	/* system symbols will not exceed this limit */

extern SYMTAB_ID 	sysSymTbl;	/* system symbol table */
extern SYMBOL		standTbl[];	/* standalone symbol table array */
extern ULONG		standTblSize;	/* symbols in standalone table */
extern SYMTAB_ID	statSymTbl;	/* system error code symbol table */
extern SYMBOL		statTbl[];	/* status string symbol table array */
extern ULONG		statTblSize;	/* status strings in status table */

#ifdef __cplusplus
}
#endif

#endif /* __INCsysSymTblh */
