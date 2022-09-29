/*
 * Copyright 1995, Silicon Graphics, Inc.
 * ALL RIGHTS RESERVED
 * 
 * UNPUBLISHED -- Rights reserved under the copyright laws of the United
 * States.   Use of a copyright notice is precautionary only and does not
 * imply publication or disclosure.
 * 
 * U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the Rights
 * in Technical Data and Computer Software clause at DFARS 252.227-7013 and/or
 * in similar or successor clauses in the FAR, or the DOD or NASA FAR
 * Supplement.  Contractor/manufacturer is Silicon Graphics, Inc.,
 * 2011 N. Shoreline Blvd. Mountain View, CA 94039-7311.
 * 
 * THE CONTENT OF THIS WORK CONTAINS CONFIDENTIAL AND PROPRIETARY
 * INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,
 * DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS STRICTLY
 * PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON
 * GRAPHICS, INC.
 */

#ident "$Id: logger.h,v 2.20 1998/09/09 17:53:58 kenmcd Exp $"

#ifndef _LOGGER_H
#define _LOGGER_H

#include "pmapi.h"

/*
 * a task is a bundle of fetches to be done together
 */
typedef struct _task {
    struct _task	*t_next;
    struct timeval	t_delta;
    int			t_state;	/* logging state */
    int			t_numpmid;
    pmID		*t_pmidlist;
    fetchctl_t		*t_fetch;
    int			t_afid;
    int			t_size;
} task_t;

extern task_t		*tasklist;	/* master list of tasks */
extern __pmLogCtl	logctl;		/* global log control */

/* config file parser states */
#define GLOBAL  0
#define INSPEC  1

/* generic error message buffer */
extern char     emess[];

/*
 * hash control for per-metric (really per-metric per-log specification)
 * -- used to establish and maintain state for ControlLog operations
 */
extern __pmHashCtl	pm_hash;

/* another hash list used for maintaining information about all metrics and
 * instances that have EVER appeared in the log as opposed to just those
 * currently being logged.  It's a history list.
 */
extern __pmHashCtl	hist_hash;

typedef struct {
    int	ih_inst;
    int	ih_flags;
} insthist_t;

typedef struct {
    pmID	ph_pmid;
    pmInDom	ph_indom;
    int		ph_numinst;
    insthist_t	*ph_instlist;
} pmidhist_t;

/* access control goo */
#define OP_LOG_ADV	0x1
#define OP_LOG_MAND	0x2
#define OP_LOG_ENQ	0x4

#define OP_NONE		0x0
#define OP_ALL		0x7

#define PMLC_SET_MAYBE(val, flag) \
	val = (val & ~0x10) | ((flag & 0x1) << 4)
#define PMLC_GET_MAYBE(val) \
	((val & 0x10) >> 4)

/* volume switch types */
#define VOL_SW_SIGHUP  0
#define VOL_SW_PMLC    1
#define VOL_SW_COUNTER 2
#define VOL_SW_BYTES   3
#define VOL_SW_TIME    4

/* flush requested via SIGUSR1 */
extern int	wantflush;

/* initial time of day from remote PMCD */
extern struct timeval	epoch;

/* yylex() gets input from here ... */
extern FILE		*fconfig;

extern int myFetch(int, pmID *, __pmPDU **);
extern void yyerror(char *);
extern void yywarn(char *);
extern int yylex(void);
extern int yyparse(void);
extern void dometric(const char *);
extern void buildinst(int *, int **, char ***, int, char *);
extern void freeinst(int *, int *, char **);
extern void log_callback(int, void *);
extern int chk_one(task_t *, pmID, int);
extern int chk_all(task_t *, pmID);
extern optreq_t *findoptreq(pmID, int);
extern int newvolume(int);
extern void disconnect(int);
#if CAN_RECONNECT
extern int reconnect(void);
#endif
extern int do_preamble(void);
extern void run_done(int,char *);
extern __pmPDU *rewrite_pdu(__pmPDU *, int);
extern int putmark(void);

#include <sys/param.h>
extern char pmlc_host[];

#define LOG_DELTA_ONCE		-1
#define LOG_DELTA_DEFAULT	-2

#endif /* _LOGGER_H */
