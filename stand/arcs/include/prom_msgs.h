#include <sys/SN/kldiag.h>

/*
 * Macros for printing in the PROM.
 * DIAG printf. Print based on diag mode setting.
 * This needs the diag mode to be passed in as parameter.
 * In most cases we can assume that the variable diag_mode
 * will be defined. Or diag_setting.
 */

#define	DG_PRINTF(_m,_s)					\
			if (_m & 				\
				((DIAG_MODE_HEAVY) 	||	\
				 (DIAG_MODE_MFG  ) 	||	\
				 (DIAG_FLAG_VERBOSE))) {	\
				printf _s ;			\
			}

#define DM_PRINTF(_s)		DG_PRINTF(diag_mode,_s)
#define DS_PRINTF(_s)		DG_PRINTF(diag_settings,_s)
#define DB_PRINTF(_s)	

/*
 * status printf. Same as diag print. The condition for
 * printing can be changed if required.
 */

#define ST_PRINTF(_m,_s)	DG_PRINTF(_m,_s)
