#ifndef CF_SM_H
#define CF_SM_H
/*
 * Copyright 1990,1991 Silicon Graphics, Inc.  All rights reserved.
 *
 *	$Revision: 1.7 $
 */

#define CFBUFSIZE	512
#define LINESIZE	CFBUFSIZE
#define NAMESIZE	64

extern void conf_station(FILE*);
extern void conf_mac(FILE*);
#endif
