#ifndef CF_SANITY_H
#define CF_SANITY_H
/*
 * Copyright 1990,1991 Silicon Graphics, Inc.  All rights reserved.
 *
 *	$Revision: 1.7 $
 */

extern int path_sanity(void);
extern int phy_sanity(SMT_MAC *, int);
extern int mac_sanity(SMT_MAC *);
extern int smtd_sanity(SMTD *);
#endif
