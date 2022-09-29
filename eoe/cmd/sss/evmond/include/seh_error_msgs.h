#ifndef __SEH_ERR_MSGS_H_
#define __SEH_ERR_MSGS_H_

#include <stdlib.h>
#ifdef __cplusplus
extern "C" {
#endif

#if VERBOSE_ERROR_MESSAGES
char *seh_major_errors[] = {
  "Error in the main loop -- unused.",
  "Initialization time error.",
  "Run-time error associated with events.",
  "Run-time error associated with api.",
  "Run-time error associated with dsm.",
  "Run-time error associated with ssdb.",
  "Run-time error associated with archiving.",
  0,
};

char *seh_common_minor_errors[] = {
  "Unable to allocate memory.",
  "Unexpected argument to a function.",
  0,
};

char *seh_init_minor_errors[] = {
  "Error initializing api.",
  "Error initializing db.",
  "Error initializing dsm.",
  "Error reading number of events.",
  "Error reading a particular event.",
  0,
};

char *seh_api_minor_errors[] = {
  "Error while xmitting to API.",
  "Error while receiving from API.",
  "Garbage received from API.",
  "Unknown version number of event.",
  "Unable to create return Q.",
  0,
};

char *seh_dsm_minor_errors[] = {
  "Error while transmitting to DSM.",
  0,
};

char *seh_db_minor_errors[] = {
  "Error while xmitting to DB.",
  "Error while receiving from DB.",
  "Bad event received from DB.",
  "Bad event number received from DB.",
  "Record missing in DB.",
  "Error writing record into DB.",
  "Missing field in DB.",
  "DB API error.",
  "Error in Function to write data.",
  "Lock table error.",
  "DB Connection Error.",
  "Write sysid into db.",
  "Reading runtime data from db.",
  0,
};

char *seh_event_minor_errors[] = {
  "Got an invalid event.",
  "Error finding event.",
  "Duplicate event.",
  "Garbage event.",
  0,
};

char *seh_archive_minor_errors[] = {
  "Error xmitting on archive port.",
  "Error receiving from archive por.",
  "Garbage received from archive port.",
  "Bad version number recieved.",
};

char **seh_major_to_minor_errors[] = {
  NULL,
  seh_init_minor_errors,
  seh_event_minor_errors,
  seh_api_minor_errors,
  seh_dsm_minor_errors,
  seh_db_minor_errors,
  seh_archive_minor_errors,
  0,
};
#endif

#ifdef __cplusplus
}
#endif

#endif /* __SEH_ERR_MSGS_H_ */
