#ifndef SMT_SUBR_H
#define SMT_SUBR_H
/*
 * Copyright 1990,1991 Silicon Graphics, Inc.  All rights reserved.
 *
 *	$Revision: 1.9 $
 */

extern char *midtoa(LFDDI_ADDR *);
extern char *sidtoa(SMT_STATIONID *);
extern int atomid(char *, LFDDI_ADDR *);
extern int atosid(char *, SMT_STATIONID *);
extern void bitswapcopy(void*, void*, int);
extern void smt_gts(struct timeval *);
extern int smt_getmid(char*, LFDDI_ADDR*);
#endif
