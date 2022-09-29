/*
 * File contains definition of error codes for the SEH only. The error code is
 * a 64-bit number. The upper 8 bits are unused for now. Probably can be used
 * by higher level modules.
 * This is how the error number looks like...
 *  o 48-55: Major number of error -- these are regular numbers.
 *  o 0-47 : Minor number of error -- these are bitmaps.
 */

#define SEH_ERROR_MAJOR(X)     ((X >> 48) & 0xff)
#define SEH_ERROR_MINOR(X)     (X & 0xffFFffFFffFFULL)
#define SEH_ERROR(MAJ,MIN)     \
     (((MAJ & 0xff) << 48) | (MIN & 0xffFFffFFffFFULL))

/*
 * Actual error codes -- MAJ stands for major, MIN stands for minor.
 */
#define SEH_MAJ_MAIN_ERR    0ULL /* Error in the main loop -- unused.     */
#define SEH_MAJ_INIT_ERR    1ULL /* Initialization time error             */
#define SEH_MAJ_EVENT_ERR   2ULL /* Run-time error associated with events */
#define SEH_MAJ_API_ERR     3ULL /* Run-time error associated with api    */
#define SEH_MAJ_DSM_ERR     4ULL /* Run-time error associated with dsm    */
#define SEH_MAJ_SSDB_ERR    5ULL /* Run-time error associated with ssdb   */
#define SEH_MAJ_ARCHIVE_ERR 6ULL /* Run-time error associated with archive*/

/*
 * Minor codes from now on. First 16 are reserved for common errors.
 */
#define SEH_MIN_ALLOC_MEM   (1ULL << 0)	/* Unable to allocate memory. */
#define SEH_MIN_INVALID_ARG (1ULL << 1)	/* Unexpected argument to a function. */

/*
 * Init errors.
 */
#define SEH_MIN_INIT_API     (1ULL << 16) /*Error initializing api */
#define SEH_MIN_INIT_DB      (1ULL << 17) /*Error initializing db  */
#define SEH_MIN_INIT_DSM     (1ULL << 18) /*Error initializing dsm */
#define SEH_MIN_INIT_EVNUM   (1ULL << 19) /*Error reading number of events */
#define SEH_MIN_INIT_EVENT   (1ULL << 20) /*Error reading a particular event.*/
#define SEH_MIN_INIT_ARCHIVE (1ULL << 21) /*Error initialize archive port. */

/*
 * api errors.
 */
#define SEH_MIN_API_XMIT   (1ULL << 16)	/* Error while xmitting to API */
#define SEH_MIN_API_RCV    (1ULL << 17)	/* Error while receiving from API */
#define SEH_MIN_API_GRB    (1ULL << 18) /* Garbage received from API */
#define SEH_MIN_API_VER    (1ULL << 19) /* Unknown version number of event */
#define SEH_MIN_API_CREATE (1ULL << 20) /* Unable to create return Q. */

/*
 * DSM errors.
 */
#define SEH_MIN_DSM_XMIT   (1ULL << 16) /* Error while transmitting to DSM */


/*
 * SSDB errors.
 */
#define SEH_MIN_DB_XMIT     (1ULL << 16)   /* Error while xmitting to DB */
#define SEH_MIN_DB_RCV      (1ULL << 17)   /* Error while receiving from DB */
#define SEH_MIN_DB_EVENT    (1ULL << 18)   /* Bad event received from DB */
#define SEH_MIN_DB_EVNUM    (1ULL << 19)   /* Bad event number received from DB */
#define SEH_MIN_DB_MISS_REC  (1ULL << 20)   /* Record missing in DB */
#define SEH_MIN_DB_WRIT_DATA (1ULL << 21)   /* Error writing record into DB */
#define SEH_MIN_DB_MISS_FLD  (1ULL << 22)   /* Missing field in DB */
#define SEH_MIN_DB_API_ERR   (1ULL << 23)   /* DB API error. */
#define SEH_MIN_DB_FUNC_ERR  (1ULL << 24)   /* Function to write data. */
#define SEH_MIN_DB_LOCK_ERR  (1ULL << 25)   /* Lock table error. */
#define SEH_MIN_DB_CONNERR   (1ULL << 26)   /* DB Connection Error */
#define SEH_MIN_DB_SYSID     (1ULL << 27)   /* Write sysid into db. */
#define SEH_MIN_DB_RUNTIME   (1ULL << 28)   /* Reading runtime data from db. */

/*
 * Event handling errors.
 */
#define SEH_MIN_EV_INVALID  (1ULL << 16)   /* Got an invalid event */
#define SEH_MIN_EV_FIND     (1ULL << 17)   /* Error finding event. */
#define SEH_MIN_EV_DUP      (1ULL << 18)   /* Duplicate event      */
#define SEH_MIN_EV_GRB      (1ULL << 19)   /* Garbage event        */
#define SEH_MIN_EV_MC       (1ULL << 20)   /* Mode change for SGM  */
#define SEH_MIN_EV_SUBSCR   (1ULL << 21)   /* Subscribe Error      */
#define SEH_MIN_EV_USUBSCR  (1ULL << 22)   /* Unsubscribe Error    */

/* 
 * Error from archive thread.
 */
#define SEH_MIN_ARCHIVE_XMIT (1ULL << 16) /*Error xmitting on archive port.*/
#define SEH_MIN_ARCHIVE_RCV  (1ULL << 17) /*Error receiving from archive port*/
#define SEH_MIN_ARCHIVE_GRB  (1ULL << 18) /*Garbage received from archive port.*/
#define SEH_MIN_ARCHIVE_VER  (1ULL << 19) /*Bad version number recieved.*/
