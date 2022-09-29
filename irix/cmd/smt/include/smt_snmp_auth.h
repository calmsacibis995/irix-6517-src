#ifndef SMT_SNMP_AUTH_H
#define SMT_SNMP_AUTH_H
/*
 * Copyright 1990,1991 Silicon Graphics, Inc.  All rights reserved.
 *
 *	$Revision: 1.7 $
 */

extern char *snmp_auth_parse(char*, int*, char*, int*, long*);
extern char *snmp_auth_build(char*, int*, char*, int*, long*, int);
#endif
