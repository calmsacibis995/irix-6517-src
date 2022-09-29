/* stateP.h - header file for string */

/* Copyright 1992 Wind River Systems, Inc. */

/*
modification history
--------------------
01b,22sep92,rrr  added support for c++
01a,08jul92,smb  written and documented.
*/

/*
DESCRIPTION

SEE ALSO: American National Standard X3.159-1989
*/

#ifndef __INCstrStatePh
#define __INCstrStatePh

#ifdef __cplusplus
extern "C" {
#endif

#define ST_CH		0x00ff
#define ST_STATE	0x0f00
#define ST_STOFF	8
#define ST_FOLD		0x8000
#define ST_INPUT	0x4000
#define ST_OUTPUT	0x2000
#define ST_ROTATE	0x1000
#define _NSTATE		16

typedef struct
    {
    const unsigned short *__table[_NSTATE];
    } __statetable;

extern __statetable __costate;		/* string collate character table */

#if FALSE     /* NOT IMPLEMENTED */
extern __statetable __mbstate; 		/* multibyte character table */
extern __statetable __wcstate; 		/* wide character table */
#endif

#ifdef __cplusplus
}
#endif

#endif /* __INCstrStatePh */
