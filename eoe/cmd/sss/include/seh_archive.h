#ifndef __ARCHIVE_H_
#define __ARCHIVE_H_

#define SEH_TO_ARCHIVE_MSGQ          ".semtoarchive"
#define ARCHIVE_TO_SEH_MSGQ          ".archivetosem"
#define ARCHIVE_MAGIC_NUMBER         0xDEADBEEF
#define ARCHIVE_VERSION_1_0          1000
#define ARCHIVE_IDS                  1
#define ARCHIVE_BUFFER_SIZE          16

/* 
 * NOTE: Try to make each one of these a different length.
 */
#define GLOBAL_CONFIG_STRING  "CONFIGURATIONGLOBAL"
#define EVENT_CONFIG_STRING   "CONFIGURATIONEVENT"
#define RULE_CONFIG_STRING    "CONFIGURATIONRULE"
#define SGM_CONFIG_STRING     "CONFIGURATIONSGM"
#define ARCHIVE_START         "ARCHIVESTART"
#define ARCHIVE_END           "ARCHIVEEND"
#define ARCHIVE_STATUS        "STATUS"
#define SEMD_QUIT             "QUIT"

#define MINIMUM_CONFIG_LENGTH           13 /* length of configuration. */

/* 
 * Structure below has a multi-faceted role.
 *   o send archive start and end messages.
 *   o send status request/results.
 */
typedef struct Archive_s {
    unsigned long  LmsgKey;	/* pid of process -- unused for now. */
    __uint64_t     aullArchive_id[ARCHIVE_IDS];	/*  */
    unsigned long  LMagicNumber; /* magic number to verify sender */
    int            iVersion;	/* version number of structure. */
    int            iDataLength;	/* length of incoming event data. */
    void          *ptrData;	/* pointer to data. */
} Archive_t;

/* 
 * Wait for SEH to signal that it is ready to archive.
 */
#define ARCHIVE_CHECK_STATE()                                   \
{                                                               \
    int i=0;                                                    \
    while(state != SSDB_ARCHIVING_STATE)                        \
      {                                                         \
	  sleep(2);                                             \
	  if(i++ == 1)                                          \
            {                                                   \
 	      /*                                                \
	       * We did not yet hear from the SEH, so timeout   \
	       * and set the archiving state to                 \
	       * SSDB_ARCHIVING_STATE ourselves.                \
	       */                                               \
              SSDB_ARCHIVE_STATE();                             \
              break;                                            \
            }                                                   \
      }                                                         \
}								  
#endif /* __ARCHIVE_H_ */
