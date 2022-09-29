/*
 * identity.c
 *	Declarations and prototypes for FFSC identity services
 */

#ifndef _IDENTITY_H_
#define _IDENTITY_H_

#include "ffsc.h"

struct in_addr;

/* Conflict Resolution Function pointer */
typedef int (*CRFPtr)(identity_t *, identity_t *);


/* Identity table functions */
STATUS	   identCleanOldEntries(timestamp_t *);
STATUS	   identClearTable(void);
int	   identEnumerateModules(modulenum_t *, int);
int	   identEnumerateRacks(rackid_t *, int);
identity_t *identFindByIPAddr(struct in_addr *);
identity_t *identFindByModuleNum(modulenum_t, bayid_t *);
identity_t *identFindByRackID(rackid_t);
identity_t *identFindByUniqueID(uniqueid_t);
identity_t *identFindConflictingIPAddr(identity_t *);
identity_t *identFindConflictingRackID(identity_t *);
identity_t *identFindMaster(void);
int	   identPrintTable(void);
STATUS	   identScanForConflicts(CRFPtr, CRFPtr);
int	   identUpdate(identity_t *);


/* Other identity functions */
STATUS identInit(void);
STATUS identPrint(identity_t *);
STATUS identSetCapabilities(unsigned int);
STATUS identSetIPAddr(struct in_addr *);
STATUS identSetModuleNums(modulenum_t[MAX_BAYS]);
STATUS identSetRackID(rackid_t);

#ifndef PRODUCTION
STATUS identParse(char *, identity_t *);
#endif  /* !PRODUCTION */

#endif  /* _IDENTITY_H_ */
