/* taskVarLib.h - header for task variables */

/* Copyright 1984-1992 Wind River Systems, Inc. */

/*
modification history
--------------------
02b,22sep92,rrr  added support for c++
02a,04jul92,jcf  cleaned up.
01f,26may92,rrr  the tree shuffle
01e,04oct91,rrr  passed through the ansification filter
		  -fixed #else and #endif
		  -changed copyright notice
01d,05oct90,shl  added ANSI function prototypes.
                 made #endif ANSI style.
                 added copyright notice.
01c,08aug90,shl  added INCtaskVarLibh to #endif.
01b,01aug90,jcf  cleanup.
01a,25jan88,jcf  written by extracting from vxLib.h v02l.
*/

#ifndef __INCtaskVarLibh
#define __INCtaskVarLibh

#ifdef __cplusplus
extern "C" {
#endif

/* task variable descriptor */

typedef struct taskVar	/* TASK_VAR */
    {
    struct taskVar *	next;	/* ptr to next task variable */
    int *		address;/* address of variable to swap */
    int			value;	/* when task is not running: save of task's val;
				 * when task is running: save of orig. val */
    } TASK_VAR;

/* function declarations */

#if defined(__STDC__) || defined(__cplusplus)

extern STATUS 	taskVarAdd (int tid, int *pVar);
extern STATUS 	taskVarDelete (int tid, int *pVar);
extern STATUS 	taskVarInit (void);
extern STATUS 	taskVarSet (int tid, int *pVar, int value);
extern int 	taskVarGet (int tid, int *pVar);
extern int 	taskVarInfo (int tid, TASK_VAR varList [], int maxVars);

#else	/* __STDC__ */

extern STATUS 	taskVarAdd ();
extern STATUS 	taskVarDelete ();
extern STATUS 	taskVarInit ();
extern STATUS 	taskVarSet ();
extern int 	taskVarGet ();
extern int 	taskVarInfo ();

#endif	/* __STDC__ */

#ifdef __cplusplus
}
#endif

#endif /* __INCtaskVarLibh */
