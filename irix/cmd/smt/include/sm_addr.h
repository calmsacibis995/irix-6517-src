#ifndef SM_ADDR_H
#define SM_ADDR_H
/*
 * Copyright 1990,1991 Silicon Graphics, Inc.  All rights reserved.
 *
 *	$Revision: 1.8 $
 */

extern LFDDI_ADDR zero_mac_addr;
extern LFDDI_ADDR unknown_mac_addr;

extern void vn_init();
extern char * vn_lookup(LFDDI_ADDR*);
extern char *fddi_sidtoa(SMT_STATIONID *, char *);
extern char *fddi_ntoa(LFDDI_ADDR *, char *);
extern LFDDI_ADDR *fddi_aton(char *, LFDDI_ADDR *);
extern int fddi_hostton(char *, LFDDI_ADDR *);
extern int fddi_ntohost(LFDDI_ADDR *, char *);

#endif
