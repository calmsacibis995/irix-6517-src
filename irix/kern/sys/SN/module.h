/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1992-1997, Silicon Graphics, Inc.          *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#ifndef __SYS_SN_MODULE_H__
#define __SYS_SN_MODULE_H__

#ifdef	__cplusplus
extern "C" {
#endif

#include <sys/systeminfo.h>
#include <sys/SN/klconfig.h>
#include <ksys/elsc.h>

#ident "$Revision: 1.11 $"

#ifdef SN0XXL
#define MODULE_MAX			64
#else
#define MODULE_MAX			32
#endif
#define MODULE_MAX_NODES		6
#define MODULE_HIST_CNT			16

typedef struct module_s module_t;

struct module_s {
    moduleid_t		id;		/* Module ID of this module        */

    lock_t		lock;		/* Lock for this structure	   */

    /* List of nodes in this module */
    cnodeid_t		nodes[MODULE_MAX_NODES];
    int			nodecnt;	/* Number of nodes in array        */

    /* Fields for Module System Controller */
    int			mesgpend;	/* Message pending                 */
    int			shutdown;	/* Shutdown in progress            */
    sema_t		thdcnt;		/* Threads finished counter        */

    elsc_t		elsc;
    lock_t		elsclock;

    time_t		intrhist[MODULE_HIST_CNT];
    int			histptr;

    int			hbt_active;	/* MSC heartbeat monitor active    */
    __uint64_t		hbt_last;	/* RTC when last heartbeat sent    */

    /* Module serial number info */
    union {
	char		snum_str[MAX_SERIAL_NUM_SIZE];	 /* used by SN0    */
	uint64_t	snum_int;			 /* used by speedo */
    } snum;
    int			snum_valid;

    int			disable_alert;
    int			count_down;
};

/* module.c */
extern module_t	       *modules[MODULE_MAX];	/* Indexed by cmoduleid_t   */
extern int		nummodules;

extern void		module_init(void);
extern module_t	       *module_lookup(moduleid_t id);

extern elsc_t	       *get_elsc(void);

extern int		get_kmod_info(cmoduleid_t cmod,
				      module_info_t *mod_info);

#ifdef	__cplusplus
}
#endif

#endif /* __SYS_SN_MODULE_H */
