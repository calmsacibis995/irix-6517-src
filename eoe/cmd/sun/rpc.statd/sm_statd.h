/*	@(#)sm_statd.h	1.1 88/04/20 4.0NFSSRC SMI	*/

/* 
 * Copyright (c) 1988 by Sun Microsystems, Inc.
 */

struct stat_chge {
	char *name;
	int state;
};
typedef struct stat_chge stat_chge;

#define SM_NOTIFY 6

#define backdir         "sm.bak"
#define statefile       "state"

extern char HA_STATE[], HA_CURRENT[], HA_BACKUP[];
extern char HA2_STATE[], HA2_CURRENT[], HA2_BACKUP[];
extern int ha_mode;
extern char hostname[];

