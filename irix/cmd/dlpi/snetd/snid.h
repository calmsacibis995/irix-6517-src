/*
 * Copyright (c) 1995 Spider Systems Limited
 *
 * This Source Code is furnished under Licence, and may not be
 * copied or distributed without express written agreement.
 *
 * All rights reserved.  Made in Scotland.
 *
 * Authors: George Wilkie
 *
 * snid.h of snet module
 *
 * SpiderFRAME-RELAY
 * Release 1.0.3 95/06/15
 * 
 * 
 */

/*
 * Subnet Identifiers
 */

#define SN_ID_LEN	4

extern unsigned long strtosnid(char *);
extern int snidtostr(unsigned long, char *);
