#ifndef __DSM_ERRORS_H_
#define __DSM_ERRORS_H_

#include "seh_errors.h"

/*
 * File contains definition of error codes for the DSM only. The error code is
 * a 64-bit number. The upper 8 bits are unused for now. Probably can be used
 * by higher level modules.
 * This is how the error number looks like...
 *  o 48-55: Major number of error -- these are regular numbers.
 *  o 0-47 : Minor number of error -- these are bitmaps.
 */

#define DSM_ERROR_MAJOR(X)     ((X >> 48) & 0xff)
#define DSM_ERROR_MINOR(X)     (X & 0xffFFffFFffFFULL)
#define DSM_ERROR(MAJ,MIN)     \
     (((MAJ & 0xff) << 48) | (MIN & 0xffFFffFFffFFULL))

/*
 * Actual error codes -- MAJ stands for major, MIN stands for minor.
 */
#define DSM_MAJ_MAIN_ERR   0ULL  /* Error in the main loop -- unused.     */
#define DSM_MAJ_INIT_ERR   1ULL  /* Initialization time error             */
#define DSM_MAJ_EVENT_ERR  2ULL  /* Run-time error associated with events */
     /* Run-time error associated with getting event from SEH    */
#define DSM_MAJ_SEH_ERR    3ULL  
     /* Run-time error associated with rules */
#define DSM_MAJ_RULE_ERR   4ULL
#define DSM_MAJ_SSDB_ERR   5ULL  /* Run-time error associated with ssdb   */
#define DSM_MAJ_ACTION_ERR 6ULL	 /* Run-time error associated with action */

/*
 * Minor codes from now on. First 16 are reserved for common errors.
 */
#define DSM_MIN_ALLOC_MEM   SEH_MIN_ALLOC_MEM   /* Unable to allocate memory. */
#define DSM_MIN_INVALID_ARG SEH_MIN_INVALID_ARG /* Unexpected argument...*/

/*
 * Init errors.
 */
#define DSM_MIN_INIT_SEH   (1ULL << 16) /* Error initializing seh interface */
#define DSM_MIN_INIT_DB    (1ULL << 17) /* Error initializing ssdb  */
#define DSM_MIN_NUM_RULE   (1ULL << 18) /* Error reading number of events */
#define DSM_MIN_DB_RULE    (1ULL << 19) /* Error reading a particular event. */
#define DSM_MIN_INIT_RULE  (1ULL << 20) /* Error with initializing rule(s) */
     
/*
 * errors with SEH communication.
 */
#define DSM_MIN_SEH_RCV    (1ULL << 16) /* Error receiving data from SEH */
#define DSM_MIN_SEH_GRB    (1ULL << 17) /* Garbage received from SEH */
#define DSM_MIN_SEH_VER    (1ULL << 18)	/* Bad version number of event */

/*
 * SSDB errors.
 */ 
#define DSM_MIN_DB_XMIT     (1ULL << 16) /* Error transmitting data to SSDB */
#define DSM_MIN_DB_RCV      (1ULL << 17) /* Error receiving data from SSDB */
#define DSM_MIN_DB_EVENT    (1ULL << 18) /* Bad event received from DB */
#define DSM_MIN_DB_EVNUM    (1ULL << 19) /* Bad event number received from DB*/
#define DSM_MIN_DB_WRIT_REC (1ULL << 21) /* Error writing record into DB */
#define DSM_MIN_DB_MISS_FLD (1ULL << 22) /* Missing field in DB */
#define DSM_MIN_DB_NORULE   (1ULL << 23) /* Rule not found in DB. */     
#define DSM_MIN_DB_API_ERR  (1ULL << 24) /* DB API error. */
     
/*
 * Event handling errors.
 */
#define DSM_MIN_EV_INVALID   (1ULL << 16)   /* Got an invalid event */
#define DSM_MIN_EV_FIND      (1ULL << 17)   /* Error finding event. */
#define DSM_MIN_EV_DUP       (1ULL << 18)   /* Duplicate event      */
#define DSM_MIN_EV_GRB       (1ULL << 19)   /* Garbage received in event data*/

/*
 * Rules related errors.
 */
#define DSM_MIN_RULE_HOST    (1ULL << 16)   /* Hostname not found in the rule.*/
#define DSM_MIN_RULE_FIND    (1ULL << 17)   /* Error finding rule. */
#define DSM_MIN_RULE_INVALID (1ULL << 18)   /* Invalid rule. */
#define DSM_MIN_RULE_DUP     (1ULL << 19)   /* Duplicate rule. */
#define DSM_MIN_RULE_DSO     (1ULL << 20)   /* Error while executing dso. */
#define DSM_MIN_RULE_USER    (1ULL << 21)   /* Unknown user to execute as. */
#define DSM_MIN_RULE_FILE    (1ULL << 22)   /* Cannot find executable file. */
#define DSM_MIN_RULE_PERM    (1ULL << 23)   /* Bad permissions with file. */
#define DSM_MIN_RULE_SUID    (1ULL << 24)   /* SUID bit was on. */

#endif
